/*
 * MarketControl.cpp
 *
 *  Created on: Jun 11, 2017
 *      Author: ondra
 */

#include "marketControl.h"

#include <couchit/couchDB.h>
#include <couchit/exception.h>
#include <imtjson/rpc.h>

namespace quark {

MarketControl::MarketControl(Value cfg)
	:ordersDb(initConfig(cfg,"orders"))
	,tradesDb(initConfig(cfg,"trades"))
{


}


couchit::Config quark::MarketControl::initConfig(Value cfgjson, StrViewA suffix) {
	couchit::Config cfg;
	cfg.authInfo.username = json::String(cfgjson["username"]);
	cfg.authInfo.password = json::String(cfgjson["password"]);
	cfg.baseUrl = json::String(cfgjson["server"]);
	cfg.databaseName = String({cfgjson["dbprefix"].getString(),suffix});
	return cfg;
}

static void notImpl(RpcRequest req) {
	req.setError(501,"Not implemented yet!");
}




void MarketControl::initRpc(RpcServer& rpcServer) {

	PMarketControl me = this;
	rpcServer.add("Order.create", me, &MarketControl::rpcOrderCreate);
	rpcServer.add("Order.modify", me, &MarketControl::rpcOrderUpdate);
	rpcServer.add("Order.cancel", me, &MarketControl::rpcOrderCancel);
	rpcServer.add("Order.get", me, &MarketControl::rpcOrderGet);
	rpcServer.add("Stream.orders",  me, &MarketControl::rpcStreamOrders);
	rpcServer.add("Stream.trades", notImpl);

}


void MarketControl::rpcOrderCreate(RpcRequest rq) {
	Value args = rq.getArgs();
	if (args.size() != 1 || args[0].type() != json::object) {
		rq.setArgError(json::undefined);
		return;
	}
	CouchDB::PConnection conn = ordersDb.getConnection("_design/orders/_update/order");
	try {
		Value res = ordersDb.requestPOST(conn,args[0],nullptr,0);
		rq.setResult(res["orderId"]);
	} catch (const couchit::RequestError &e) {
		rq.setError(e.getCode(), e.getMessage(),e.getExtraInfo());
	}
}

void MarketControl::rpcOrderUpdate(RpcRequest rq) {
	Value args = rq.getArgs();
	if (args.size() != 2 || args[0].type() != json::string || args[1].type() != json::object) {
		rq.setArgError(json::undefined);
		return;
	}
	CouchDB::PConnection conn = ordersDb.getConnection("_design/orders/_update/order");
	conn->add(args[0].getString());
	try {
		Value res = ordersDb.requestPUT(conn,args[1],nullptr,0);
		rq.setResult(res["orderId"]);
	} catch (const couchit::RequestError &e) {
		rq.setError(e.getCode(), e.getMessage(),e.getExtraInfo());
	}
}

void MarketControl::rpcOrderCancel(RpcRequest rq) {
	static Value chkargs (json::array,{"string"});
	if (!rq.checkArgs(chkargs)) return rq.setArgError();
	Value args = rq.getArgs();
	CouchDB::PConnection conn = ordersDb.getConnection("_design/orders/_update/order");
	conn->add(args[0].getString());
	try {
		Value res = ordersDb.requestDELETE(conn,nullptr,0);
		rq.setResult(res["orderId"]);
	} catch (const couchit::RequestError &e) {
		rq.setError(e.getCode(), e.getMessage(),e.getExtraInfo());
	}
}

void MarketControl::rpcOrderGet(RpcRequest rq) {
	static Value chkargs (json::array,{"string"});
	if (!rq.checkArgs(chkargs)) return rq.setArgError();
	Value args = rq.getArgs();
	try {
		Value res = ordersDb.get(args[0].getString());
		rq.setResult(res);
	} catch (const couchit::RequestError &e) {
		rq.setError(e.getCode(), e.getMessage(),e.getExtraInfo());
	}
}

class MarketControl::BasicFeed: public FeedControl {
public:
	BasicFeed(couchit::CouchDB &db, Value since, RpcRequest rq, String streamName):FeedControl(db,since),rq(rq),streamName(streamName) {}
	virtual void init() override {
	}
	virtual void onEvent(Value v) override {
		rq.sendNotify(streamName,{v["seq"],v["doc"]});
	}
	~BasicFeed() {
		stop();
	}

protected:
	RpcRequest rq;
	String streamName;
};


void MarketControl::rpcStreamOrders(RpcRequest rq) {
	static Value turnOffArgs = Value(json::array,{false});
	static Value turnOnArgs = {true,"string"};
	if (rq.checkArgs(turnOffArgs)) {

		ordersFeed = nullptr;
		rq.setResult(true);


	} else if (rq.checkArgs(turnOnArgs)) {
		ordersFeed = new BasicFeed(ordersDb, rq.getArgs()[1], rq, "order");
		ordersFeed->start();
		rq.setResult(true);


	} else {
		rq.setArgError();
	}
}


void MarketControl::FeedControl::stop() {

	if (!stopped) {
		feed.cancelWait();
		thr.join();
		stopped = true;
	}
}

void MarketControl::FeedControl::start() {
	bool initWait = false;
	std::condition_variable initWaitCond;
	std::mutex lock;

	thr = std::thread([&]{

		init();

		{
		std::unique_lock<std::mutex> _(lock);
		initWait = true;
		initWaitCond.notify_all();
		}

		try {
			feed >> [&](Value x) {
				onEvent(x);
				return true;
			};
		} catch (couchit::CanceledException &) {

		}
	});

	std::unique_lock<std::mutex> _(lock);
	initWaitCond.wait(_,[&]{return initWait;});
	stopped = false;
}

MarketControl::FeedControl::FeedControl(CouchDB& db, Value since)
	:feed(db.createChangesFeed()), stopped(true)
{
	feed.since(since);
	feed.includeDocs(true);
}

}



/* namespace quark */


