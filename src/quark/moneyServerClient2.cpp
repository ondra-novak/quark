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




MoneyServerClient2::MoneyServerClient2(
		ResyncFn resyncFn,
		String addr, String signature, String asset, String currency, String firstTradeId, bool logTrafic)
	:resyncFn(resyncFn)
	,addr(addr)
	,signature(signature)
	,asset(asset)
	,currency(currency)
	,firstTradeId(firstTradeId)
	,client(new MyClient(addr,*this))
	,inited(false)
{
	client->enableLogTrafic(logTrafic);
	client->setRecvTimeout(30000);
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
			("asset",total.asset)
			("marginLong",total.marginLong)
			("marginShort",total.marginShort)
			("posLong",total.posLong)
			("posShort",total.posShort);


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
	return false;



}

class CancelException {};

void MoneyServerClient2::reportTrade(Value prevTrade, const TradeData& data) {

	 connectIfNeed();
	 try {
		 reportTrade2(prevTrade, data);
	 } catch (CancelException &e) {
		 //ignore here - we cannot do anything with it
	 }

}
void MoneyServerClient2::reportTrade2(Value prevTrade, const TradeData& data) {

	RefCntPtr<MyClient> c(client);
	if (c->isConnected()) {
		(*c)("CurrencyBalance.trade", Object("trade_id",data.id)
											("prev_trade_id", prevTrade)
											("timestamp",data.timestamp)
											("asset",data.size)
											("currency",data.price)
											("buyer",Object("user_id",data.buyer.userId)
														   ("context",data.buyer.context))
											("seller",Object("user_id",data.seller.userId)
														   ("context",data.seller.context))
											("taker",data.dir == OrderDir::buy?"buyer":"seller"))

		>> [c](const RpcResult &res){
			if (res.isError()) {
				handleError(c, "CurrencyBalance.trade", res);
			}
		};
		lastReportedTrade = data.id;
	} else {
		throw CancelException();
	}
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
};



void MoneyServerClient2::connectIfNeed() {
	if (!client->isConnected()) {
		if (client->connect(addr)) {

			time_t now;
			time(&now);
			if (now - lastDropTime < 10) {
				retryCounter++;
				if (retryCounter == 10) {
					try {
						throw std::runtime_error("MoneyServer client - too many connection drops, this is fatal");
					} catch(...) {
						unhandledException();
					}
				}
				std::this_thread::sleep_for(std::chrono::seconds(retryCounter));
			} else {
				retryCounter=0;
			}

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
				Value lastSyncId = r["last_trade_id"];
				Value version = r["version"];


				if (lastSyncId.getString() == "" && firstTradeId != "") {
					lastSyncId = firstTradeId;
				}

				logInfo({"Initialized RPC client, version, lastId", version, lastSyncId, retryCounter});

				time(&now);
				lastDropTime = now;



				try {
					ResyncStream resyncStream(*this);
					resyncFn(resyncStream, lastSyncId, lastReportedTrade);
				} catch (CancelException &e) {
					//continue disconnected
				}  catch (...) {
					unhandledException();
				}
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
