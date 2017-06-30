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

MoneyServerClient::MoneyServerClient(String addr):addr(addr),idcounter(0) {}

MoneyServerClient::~MoneyServerClient() {
	stopWorker();
}

void MoneyServerClient::adjustBudget(json::Value , OrderBudget& ) {

}

bool MoneyServerClient::allocBudget(json::Value user, OrderBudget total, Callback callback) {

	sendRequest("allocBudget",
			{user,Object("asset", total.asset)
						("currency", total.currency)
						("marginLong", total.marginLong)
						("marginShort", total.marginShort)
						("posLong", total.posLong)
						("posShort", total.posShort)
			}, [=](Response resp) {
				if (resp.error.defined()) {
					handleError(resp);
				}else {
					callback(resp.result.getBool());
				}
			},true);

	return false;

}

Value MoneyServerClient::reportTrade(Value prevTrade, const TradeData& data) {
	if (prevTrade != lastTrade) return lastTrade;
	sendRequest("trade",Object("prevTrade", prevTrade)
							  ("tradeId", data.id)
							  ("price", data.price)
							  ("size",data.size)
							  ("timestamp",(std::size_t)data.timestamp)
		, [=](Response resp) {
			if (resp.error.defined()) handleError(resp);

	}, false);
	lastTrade = data.id;
	return lastTrade;

}

bool MoneyServerClient::reportBalanceChange(const BalanceChange& data) {
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
	worker.join();
}

void MoneyServerClient::connect() {

	if (curConn != nullptr) {
		cancelFn();
		connlock.unlock();
		worker.join();
		connlock.lock();
	}

	curConn = NetworkConnection::connect(addr, 1024);

	cancelFn = curConn->createCancelFunction();
	worker = std::thread([=]{

		while (curConn->waitForInput(-1)) {

			try {
				Value resp = Value::parse(BufferedRead<InputStream>(InputStream(curConn)));
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
			} catch (...) {
				Sync _(connlock);
				curConn = nullptr;
				cancelFn = nullptr;
				break;
			}
		}

	});

}

MoneyServerClient::PendingRequests::value_type MoneyServerClient::registerRequest(String method,Value args, ResponseCb callback, bool canRepeat) {

	std::size_t id ;
	Sync _(maplock);
	id = ++idcounter;

	Value jreq = Object("method",method)
			("params", args)
			("id",id);
	PendingRequests::value_type x(id, ReqInfo(jreq, callback,canRepeat));
	pendingRequest.insert(x);
	return x;
}

void MoneyServerClient::sendRequest(const Value& rq) {
	Sync _(reqlock);
	PNetworkConection myconn = nullptr;
	while (myconn == nullptr)
	{
		Sync _(connlock);
		myconn = curConn;
	}

	rq.serialize(BufferedWrite<OutputStream>(OutputStream(curConn)));
	if (myconn->isTimeout()) {
		logWarn({"Timeout writting to", addr, rq});
		connect();
	} else if (myconn->getLastSendError()) {
		logWarn({"Error wriiting to", addr, curConn->getLastSendError(), rq});
		connect();
	}

}

void MoneyServerClient::sendRequest(String method, Value args, ResponseCb callback, bool canRepeat) {
	PendingRequests::value_type req = registerRequest(method,args, callback, canRepeat);
	sendRequest(req.second.request);
}

void MoneyServerClient::sendNotify(String method, Value args) {
	Value jreq = Object("method",method)
					   ("params", args)
			           ("id",nullptr);

	sendRequest(jreq);
}

void MoneyServerClient::handleError(const Response& resp) {

}

void MoneyServerClient::onNotify(Value resp) {
}

} /* namespace quark */
