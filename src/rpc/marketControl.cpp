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
	,orderControl(ordersDb)
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
	rpcServer.add("Stream.trades", me, &MarketControl::rpcStreamTrades);
	rpcServer.add("Stream.lastId", me, &MarketControl::rpcStreamLastId);

}


void MarketControl::rpcOrderCreate(RpcRequest rq) {
	static Value chkargs (json::array,{Object("%","any")});
	if (!rq.checkArgs(chkargs)) return rq.setArgError();
	Value args = rq.getArgs();
	try {
		rq.setResult(orderControl.create(args[0]));
	} catch (ValidatonError &e) {
		rq.setError(400,"Validation failed", e.getError());
	}
}

void MarketControl::rpcOrderUpdate(RpcRequest rq) {
	static Value chkargs1 = {"string",Object("%","any")};
	static Value chkargs2 = {"string",Object("%","any"), "string"};
	Value args = rq.getArgs();
	try {
		if (rq.checkArgs(chkargs1)) {
			rq.setResult(orderControl.modify(args[0],args[1],json::undefined));
		} else if (rq.checkArgs(chkargs2)) {
			rq.setResult(orderControl.modify(args[0],args[1],args[2]));
		} else {
			rq.setArgError();
		}
	} catch (ValidatonError &e) {
		rq.setError(400,"Validation failed", e.getError());
	} catch (OrderNotFoundError &e) {
		rq.setError(404,"Order not found");
	} catch (ConflictError &e) {
		rq.setError(409,"Conflict",e.getActualDoc());
	}
}

void MarketControl::rpcOrderCancel(RpcRequest rq) {
	static Value chkargs (json::array,{"string"});
	if (!rq.checkArgs(chkargs)) return rq.setArgError();
	Value args = rq.getArgs();
	try {
		rq.setResult(orderControl.cancel(args[0]));
	} catch (OrderNotFoundError &e) {
		rq.setError(404,"Order not found");
	}
}

void MarketControl::rpcOrderGet(RpcRequest rq) {
	static Value chkargs (json::array,{"string"});
	if (!rq.checkArgs(chkargs)) return rq.setArgError();
	Value args = rq.getArgs();
	try {
		rq.setResult(orderControl.getOrder(args[0]));
	} catch (const couchit::RequestError &e) {
		rq.setError(404,"Order not found");
	}
}

class MarketControl::BasicFeed: public FeedControl {
public:
	BasicFeed(couchit::CouchDB &db, Value since, RpcRequest rq, String streamName):FeedControl(db,since),rq(rq),streamName(streamName) {}
	virtual void init() override {
	}
	virtual void onEvent(Value v) override {
		rq.sendNotify(streamName,{v["seq"].toString(),v["doc"]});
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
	static Value turnOnArgs = {true,{"string","optional"} };
	if (rq.checkArgs(turnOffArgs)) {

		ordersFeed = nullptr;
		rq.setResult(true);


	} else if (rq.checkArgs(turnOnArgs)) {
		Value since = rq.getArgs()[1];
		ordersFeed = new BasicFeed(ordersDb, since, rq, "order");
		ordersFeed->start();
		rq.setResult(true);


	} else {
		rq.setArgError();
	}
}

void MarketControl::rpcStreamTrades(RpcRequest rq) {
	static Value turnOffArgs = Value(json::array,{false});
	static Value turnOnArgs = {true,{"string","optional"} };
	if (rq.checkArgs(turnOffArgs)) {

		ordersFeed = nullptr;
		rq.setResult(true);


	} else if (rq.checkArgs(turnOnArgs)) {
		Value since = rq.getArgs()[1];
		ordersFeed = new BasicFeed(tradesDb, since, rq, "trade");
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
	if (since.defined())
		feed.since(since);
	feed.setTimeout(-1);
	feed.includeDocs(true);
}

void MarketControl::rpcStreamLastId(RpcRequest rq) {
	if (!rq.checkArgs(json::array)) return rq.setArgError();

	rq.setResult(Object("orders",ordersDb.getLastSeqNumber())
					   ("trades",tradesDb.getLastSeqNumber())
			);

}

}



/* namespace quark */


