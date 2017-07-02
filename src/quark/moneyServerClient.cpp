/*
 * moneyServerClient.cpp
 *
 *  Created on: 30. 6. 2017
 *      Author: ondra
 */


#include <imtjson/binary.h>
#include <couchit/minihttp/buffered.h>
#include "moneyServerClient.h"

#include "logfile.h"

#include "orderBudget.h"

namespace quark {

MoneyServerClient::MoneyServerClient(PMoneySvcSupport support, String addr, String signature)
:support(support),addr(addr),idcounter(0),signature(signature),lastTrade(nullptr) {}

MoneyServerClient::~MoneyServerClient() {
	stopWorker();
}

void MoneyServerClient::adjustBudget(json::Value , OrderBudget& ) {

}

class MoneyServerClient::BudgetAllocCallback: public RefCntObj {
public:
	BudgetAllocCallback(const IMoneySrvClient::Callback &cb, MoneyServerClient *owner):cb(cb),owner(owner) {}
	BudgetAllocCallback(const BudgetAllocCallback &other)=delete;
	BudgetAllocCallback(BudgetAllocCallback &&other):cb(std::move(other.cb)),owner(std::move(other.owner)) {}
	~BudgetAllocCallback() {
		if (cb != nullptr)
			cb(IMoneySrvClient::allocTryAgain);
	}
	void operator()(MoneyServerClient::Response resp) {
		Callback c = cb;
		cb = nullptr;
		if (!resp.error.isNull()) {
			c(IMoneySrvClient::allocError);
			owner->handleError(resp);
		} else {
			c(resp.result.getBool()?allocOk:allocReject);
		}
	}

	static ResponseCb getAsCallback(const IMoneySrvClient::Callback &cb, RefCntPtr<MoneyServerClient> owner) {
		RefCntPtr<BudgetAllocCallback>  cptr = new BudgetAllocCallback(cb,owner);
		return [cptr](Response resp) {
			cptr->operator ()(resp);
		};
	}

protected:
	MoneyServerClient *owner;
	Callback cb;
};

bool MoneyServerClient::allocBudget(json::Value user, OrderBudget total, Callback callback) {


	ResponseCb fnCb = BudgetAllocCallback::getAsCallback(callback, this);
	sendRequest("allocBudget",
			{user,Object("asset", total.asset)
						("currency", total.currency)
						("marginLong", total.marginLong)
						("marginShort", total.marginShort)
						("posLong", total.posLong)
						("posShort", total.posShort)
			},fnCb,true);

	return false;

}

void MoneyServerClient::reportTrade(Value prevTrade, const TradeData& data) {
	//remember last reported trade id
	lastStoredTrade = data.id;
	//we are currently unable to send trade, because last trade is not known
	if (lastTrade.isNull()) return;
	//now send the trade
	sendRequest("trade",Object("prevTrade", prevTrade)
							  ("tradeId", data.id)
							  ("price", data.price)
							  ("size",data.size)
							  ("timestamp",(std::size_t)data.timestamp)
		, [=](Response resp) {
			if (!resp.error.isNull()) handleError(resp);

	}, false);
	//store trade id
	lastTrade = data.id;


}

void MoneyServerClient::reportBalanceChange(const BalanceChange& data) {
	sendNotify("balanceChange", Object("user", data.user)
									  ("context", OrderContext::str[data.context])
									  ("currencyChange",data.currencyChange)
									  ("assetChange",data.assetChange)
									  ("trade",data.trade)
									  ("taker", data.taker)
									  ("feeType","normal"));
}

void MoneyServerClient::commitTrade(Value tradeId) {
	sendNotify("commitTrade", Value(array,{tradeId}));
}


void MoneyServerClient::stopWorker() {
	{
		Sync _(connlock);
		if (cancelFn) cancelFn();
	}
	if (worker.joinable())
		worker.join();

}

void MoneyServerClient::disconnect() {
	Sync _(connlock);
	if (cancelFn) cancelFn();
	worker.join();
	curConn = nullptr;
}

void MoneyServerClient::login() {

	RefCntPtr<MoneyServerClient> me(this);
	if (!needLogin) return;
	PendingRequests oldReq;
	{
		Sync _(maplock);
		std::swap(oldReq,pendingRequest);
		needLogin = false;
	}

	Value initreq = registerRequestLk("init", Object("market",signature), [me](Response resp) {
		if (!resp.error.isNull()) me->handleError(resp);
		me->support->dispatch([me,resp] {
			me->lastTrade = resp.result;
			if (me->lastTrade != me->lastStoredTrade) {
				me->support->resync(PMoneySrvClient::staticCast(me), me->lastTrade, me->lastStoredTrade);
			}
		});
	},false);

	sendRequestLk(initreq);

	for (auto &&item: oldReq) {
		if (item.second.canRepeat) {
			Value r = registerRequestLk(item.second.method, item.second.args, item.second.callback, true);
			sendRequestLk(r);
		}
	}
}

bool MoneyServerClient::connect(int trynum) {

	if (trynum == 0 && !pendingConnect.try_lock()) return false;
	try {
		if (curConn == nullptr) {
			PNetworkConection c = NetworkConnection::connect(addr, 1024);
			if (c == nullptr) {
				int err;
				logError({"Failed to connect moneyserver", addr, err});
				if (trynum >= 10) {
					throw std::runtime_error(String({"Failed to connect moneyserver after 10 tries: ", Value(addr).getString()," - errno: ", Value(err).getString()}).c_str());
				}
				RefCntPtr<MoneyServerClient> me(this);
				std::thread thr([me,trynum] {
					std::this_thread::sleep_for(std::chrono::seconds(1));
					me->support->dispatch([me,trynum]{
						if (me->isShared()) {
							me->connect(trynum+1);
						} else {
							me->pendingConnect.unlock();
						}
					});
				});
				thr.detach();
				return false;
			}

			needLogin = true;
			curConn = c;
			worker = std::thread([=]{workerProc(c);});
			login();
			pendingConnect.unlock();

		}
		return true;
	} catch (...) {
		pendingConnect.unlock();
		throw;
	}
}

Value MoneyServerClient::registerRequestLk(String method,Value args, ResponseCb callback, bool canRepeat) {

	std::size_t id ;
	id = ++idcounter;

	Value jreq = Object("method",method)
			("params", args)
			("id",id);
	PendingRequests::value_type x(id, ReqInfo(method, args, callback,canRepeat));
	Sync _(maplock);
	pendingRequest.insert(x);
	return jreq;
}

PNetworkConection MoneyServerClient::getConnection() {
	Sync _(connlock);
	return curConn;
}

void MoneyServerClient::sendRequestLk(const Value& rq) {
	PNetworkConection conn = getConnection();
	if (conn == nullptr) {
		if (!connect()) return;
		conn = getConnection();
	}
	if (conn == nullptr) return;

	std::size_t wr;
	OutputStream out(curConn);
	rq.serialize(BufferedWrite<OutputStream>(out));
	out((unsigned char *)"\n",1,&wr);
	if (conn->isTimeout()) {
		logWarn({"Timeout writting to", addr, rq});
		disconnect();
		connect();
	} else if (conn->getLastSendError()) {
		logWarn({"Error wriiting to", addr, curConn->getLastSendError(), rq});
		disconnect();
		connect();
	}

}

void MoneyServerClient::sendRequest(String method, Value args, ResponseCb callback, bool canRepeat) {
	Value req = registerRequestLk(method,args, callback, canRepeat);
	sendRequestLk(req);
}

void MoneyServerClient::sendNotify(String method, Value args) {
	Value jreq = Object("method",method)
					   ("params", args)
			           ("id",nullptr);

	sendRequestLk(jreq);
}

void MoneyServerClient::handleError(const Response& resp) {
	RefCntPtr<MoneyServerClient> me(this);
	logError({"Money server error: ", addr, resp.error});
	needLogin = true;
	support->dispatch([=]{me->login();});
}

void MoneyServerClient::onNotify(Value resp) {
	logError({"notify is not implemented yet", resp});
}


void MoneyServerClient::workerProc(PNetworkConection conn) {

	while (conn->waitForInput(-1)) {

		InputStream in(conn);
		BinaryView b = in(0);
		if (b.length == 1 && isspace(b[0])) {
			in(1);
			continue;
		}
		try {
			Value resp = Value::parse(BufferedRead<InputStream>(InputStream(conn)));
			if (resp["method"].defined()) {
				onNotify(resp);
			} else {
				ResponseCb cb;
				std::size_t id = resp["id"].getUInt();
				{
					Sync _(maplock);
					auto f = pendingRequest.find(id);
					if (f == pendingRequest.end()) continue;
					cb = f->second.callback;
					pendingRequest.erase(f);
				}
				if (cb != nullptr) {
					Response r;
					r.error = resp["error"];
					r.id = id;
					r.result = resp["result"];
					cb(r);
				}
			}
		} catch (const std::exception &e) {
			logError({"Money server rcv error: - reconnect ", e.what()});
			Sync _(connlock);
			curConn = nullptr;
			cancelFn = nullptr;
			break;
		}
	}

}


} /* namespace quark */
