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
	:signature(signature)
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

	RefCntPtr<MyClient> c(client);
	Value params = Object
			("user_id",user)
			("currency",total.currency)
			("amount",total.asset)
			("marginLong",total.marginLong)
			("marginShort",total.marginShort)
			("amountLong",total.posLong)
			("amountShort",total.posShort);


	callWithRetry(client, support, "CurrencyBalance.block_money", params,
			[callback](const RpcResult &res) {


		if (res.isError()) {
			callback(allocError);
		} else {
			if (Value(res)["success"].getBool()) {
				callback(allocOk);
			} else {
				callback(allocReject);
			}
		}


	});



}

void MoneyServerClient2::reportTrade(Value prevTrade, const TradeData& data) {

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

}

void MoneyServerClient2::reportBalanceChange(const BalanceChange& data) {
	RefCntPtr<MyClient> c(client);

	Value params = Object("trade_id",data.trade)
												("user_id", data.user)
												("context",OrderContext::str[data.context])
												("amount",data.assetChange)
												("currency",data.currencyChange)
												("taker",data.taker);

	(*c)("CurrencyBalance.change", params)

	>> [c,params](const RpcResult &res){
		if (res.isError() || Value(res)["success"].getBool()==false) {
			try {
				throw RuntimeError(Object("desc","Cannot change balance of user")
										 ("request",params)
										 ("response",res));
			} catch (...) {
				unhandledException();
			}
		}
	};

}


void MoneyServerClient2::commitTrade(Value tradeId) {
	RefCntPtr<MyClient> c(client);

	Value params = Object("sync_id",tradeId);
	(*c)("CurrencyBalance.commit_trade", params)

	>> [c,params](const RpcResult &res){
		if (res.isError() || Value(res)["success"].getBool()==false) {
			try {
				throw RuntimeError(Object("desc","Commit trade failed")
										 ("request",params)
										 ("response",res));
			} catch (...) {
				unhandledException();
			}
		}
	};
}

MoneyServerClient2::MyClient::MyClient(String addr,
		MoneyServerClient2& owner):RpcClient(addr),owner(owner),closed(false) {
}

void MoneyServerClient2::MyClient::onInit() {
	owner.onInit();
}

void MoneyServerClient2::MyClient::onNotify(const Notify& ntf) {
	owner.onNotify(ntf);
}

void MoneyServerClient2::onInit() {
	RefCntPtr<MoneyServerClient2> me(this);
	(*client)("CurrencyBalance.init",Object
			("signature",signature)
			("asset",asset)
			("currency",currency))
	>> [me](const RpcResult &res) {

		if (res.isError()) {
			if (res.defined()) {
				handleError(me->client,"init", res);
			}
		} else {
			Value r(res);
			Value lastSyncId = r["last_sync_id"];
			Value version = r["version"];

			logInfo({"Initialized RPC client, version, lastId", version, lastSyncId});


		}

	};
}

void MoneyServerClient2::onNotify(const Notify& ntf) {
}

void MoneyServerClient2::handleError(MyClient *c, StrViewA method, const RpcResult& res)
{
	logError({method, "Money server error, dropping connection", c->getAddr(), Value(res)});
	c->disconnect(false);

}

}
