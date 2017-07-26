/*
 * moneyServerClient2.cpp
 *
 *  Created on: 20. 7. 2017
 *      Author: ondra
 */




#include "moneyServerClient2.h"

#include <couchit/minihttp/buffered.h>
#include <imtjson/object.h>

#include "../common/runtime_error.h"

#include "error.h"


#include "logfile.h"
#include "orderBudget.h"

namespace quark {




MoneyServerClient2::MoneyServerClient2(PMoneySvcSupport support,
		String addr, String signature, String asset, String currency)
	:support(support)
	,addr(addr)
	,signature(signature)
	,asset(asset)
	,currency(currency)
	,client(new MyClient(addr,*this))
	,inited(false)
{
}

MoneyServerClient2::~MoneyServerClient2() {
	client->close();
}

void MoneyServerClient2::adjustBudget(json::Value ,
		OrderBudget& ) {
	//emoty
}

template<typename Fn>
void MoneyServerClient2::callWithRetry(RefCntPtr<MyClient> client,PMoneySvcSupport supp,  String methodName, Value params, Fn callback) {

	(*client)(methodName, params) >>
			[=](RpcResult res) {
		if (res.isError()) {
			if (res.defined()) {
				handleError(client,methodName,res);
			}
			if (!client->isClosed()) {
				supp->dispatch([=]{callWithRetry(client,supp,methodName,params,callback);});
			} else {
				callback(res);
			}
		} else {
			callback(res);
		}
	};

}

bool MoneyServerClient2::allocBudget(json::Value user, OrderBudget total,
		Callback callback) {

	connectIfNeed();
	RefCntPtr<MyClient> c(client);
	Value params = Object
			("user_id",user)
			("currency",total.currency)
			("amount",total.asset)
			("marginLong",total.marginLong)
			("marginShort",total.marginShort)
			("amountLong",total.posLong)
			("amountShort",total.posShort);


	(*client)("CurrencyBalance.block_money", params)
			>> [c,callback](const RpcResult &res) {


		if (res.isError()) {
			if (res.defined())
				handleError(c,"CurrencyBalance.block_money", res);
			callback(allocTryAgain);
		} else {
			if (Value(res)["success"].getBool()) {
				callback(allocOk);
			} else {
				callback(allocReject);
			}
		}


	};



}

void MoneyServerClient2::reportTrade(Value prevTrade, const TradeData& data) {

	 connectIfNeed();
	 reportTrade2(prevTrade, data);

}
void MoneyServerClient2::reportTrade2(Value prevTrade, const TradeData& data) {

	RefCntPtr<MyClient> c(client);
	(*c)("CurrencyBalance.trade", Object("trade_id",data.id)
										("prev_trade_id", prevTrade)
										("timestamp",data.timestamp)
										("price",data.price)
										("amount",data.size)
										("type",OrderDir::str[data.dir]))

	>> [c](const RpcResult &res){
		if (res.isError() || Value(res)["success"].getBool()==false) {
			handleError(c, "trade", res);
		}
	};
	lastReportedTrade = data.id;
}

void MoneyServerClient2::reportBalanceChange(const BalanceChange& data) {
	RefCntPtr<MyClient> c(client);
	//no connect here - if disconnected, request will be discarded
	Value params = Object("trade_id",data.trade)
												("user_id", data.user)
												("context",OrderContext::str[data.context])
												("asset",data.assetChange)
												("currency",data.currencyChange)
												("taker",data.taker);

	(*c)("CurrencyBalance.change", params)

	>> [c,params](const RpcResult &res){
		if (res.isError()) {
			//no response, it is ok - (disconnected)
			if (!res.defined()) return;
			else handleError(c,"CurrencyBalance.change", res);
		}
		else {
			if (Value(res)["success"].getBool()==false) {
				try {
					throw RuntimeError(Object("desc","Cannot change balance of user")
											 ("request",params)
											 ("response",res));
				} catch (...) {
					unhandledException();
				}
			}
			//everything is ok
		}

	};

}


void MoneyServerClient2::commitTrade(Value tradeId) {
	RefCntPtr<MyClient> c(client);
	//no connect here - if disconnected, request will be discarded

	Value params = Object("sync_id",tradeId);
	(*c)("CurrencyBalance.commit_trade", params)

	>> [c,params](const RpcResult &res){
		if (res.isError()) {
			//no response, it is ok - (disconnected)
			if (!res.defined()) return;
			else handleError(c,"CurrencyBalance.commit_trade", res);
		}
		else {
			if (Value(res)["success"].getBool()==false) {
				try {
					throw RuntimeError(Object("desc","Commit trade failed, this is fatal")
											 ("request",params)
											 ("response",res));
				} catch (...) {
					unhandledException();
				}
			}
			//everything is ok
		}

	};
}

MoneyServerClient2::MyClient::MyClient(String addr,
		MoneyServerClient2& owner):owner(owner),closed(false) {
}

void MoneyServerClient2::MyClient::onInit() {
	owner.onInit();
}

void MoneyServerClient2::MyClient::onNotify(const Notify& ntf) {
	owner.onNotify(ntf);
}

void MoneyServerClient2::onInit() {
	//empty
}

void MoneyServerClient2::onNotify(const Notify& ntf) {
	//empty
}

class MoneyServerClient2::ResyncStream: public ITradeStream {
public:
	MoneyServerClient2 &owner;
	ResyncStream(MoneyServerClient2 &owner):owner(owner) {}
	virtual void reportTrade(Value prevTrade, const TradeData &data) {
		owner.reportTrade2(prevTrade, data);
	}
	virtual void reportBalanceChange(const BalanceChange &data) {
		owner.reportBalanceChange(data);
	}
	virtual void commitTrade(Value tradeId) {
		owner.commitTrade(tradeId);
	}
};



void MoneyServerClient2::connectIfNeed() {
	if (!client->isConnected()) {
		if (client->connect(addr)) {

			RpcResult initres = (*client)("CurrencyBalance.init", Object
					("signature",signature)
					("asset",asset)
					("currency",currency));
			if (initres.isError()) {
				if (initres.defined()) {
					handleError(client,"CurrencyBalance.init", initres);
				}
			} else {
				Value r(initres);
				Value lastSyncId = r["last_sync_id"];
				Value version = r["version"];

				logInfo({"Initialized RPC client, version, lastId", version, lastSyncId});

				ResyncStream resyncStream(*this);
				support->resync(resyncStream, lastSyncId, lastReportedTrade);
			}

		} else {
			//failed connect
			//nothing here - commands send to disconnected client are rejected through callback
		}
	}
}

void MoneyServerClient2::handleError(MyClient *c, StrViewA method, const RpcResult& res)
{
	logError({method, "Money server error, dropping connection", c->getAddr(), Value(res)});
	c->disconnect(false);
}

}
