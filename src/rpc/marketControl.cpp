/*
 * MarketControl.cpp
 *
 *  Created on: Jun 11, 2017
 *      Author: ondra
 */

#include "marketControl.h"

#include <couchit/couchDB.h>
#include <couchit/exception.h>
#include <couchit/query.h>
#include <imtjson/rpc.h>
#include "../common/config.h"

namespace quark {

MarketControl::MarketControl(Value cfg, StrViewA dbname)
	:ordersDb(initCouchDBConfig(cfg,dbname,"-orders"))
	,tradesDb(initCouchDBConfig(cfg,dbname,"-trades"))
	,posDb(initCouchDBConfig(cfg,dbname,"-positions"))
	,orderControl(ordersDb)
{


}


static void notImpl(RpcRequest req) {
	req.setError(501,"Not implemented yet!");
}


bool MarketControl::testDatabase() {
	CouchDB::PConnection conn = ordersDb.getConnection("");
	try {
		ordersDb.requestGET(conn,nullptr,0);
		return true;
	} catch (const couchit::RequestError &e) {
		if (e.getCode() == 404) return false;
		throw;
	}
}

void MarketControl::rpcConfigGet(RpcRequest rq) {

	rq.setResult(ordersDb.get("settings",CouchDB::flgCreateNew));

}

void MarketControl::rpcConfigSet(RpcRequest rq) {
	static Value args = Value::fromString(

			"[{"
			   "\"assetSign\": \"string\","
			   "\"currencySign\": \"string\","
			   "\"granuality\": [[\">\",0]],"
			   "\"maxBudget\": [[\">\",0]],"
			   "\"maxPrice\": [[\">\",0]],"
			   "\"maxSize\": [[\">\",0]],"
			   "\"maxSlippagePct\": [[\">=\",0]],"
			   "\"maxSpreadPct\": [[\">=\",0]],"
			   "\"minPrice\": [[\">\",0]],"
			   "\"minSize\": [[\">\",0]],"
			   "\"pipSize\":[[\">\",0]],"
			   "\"moneyService\": [{"
				   	   "\"type\":\"'mockup\","
				   	   "\"latency\":[[\">=\",0,\"integer\"]],"
				   	   "\"maxBudget\": {"
				   			   "\"asset\":[[\">=\",0]],"
				   			   "\"currency\":[[\">=\",0]],"
				   			   "\"marginLong\":[[\">=\",0]],"
				   			   "\"marginShort\":[[\">=\",0]]"
				   	   "}"
			   "}]"
			"},\"string\"]");

	if (!rq.checkArgs(args)) return rq.setArgError();
	couchit::Document newDoc;
	newDoc.setBaseObject(rq.getArgs()[0]);
	newDoc.setID("settings");
	newDoc.setRev(rq.getArgs()[1]);
	ordersDb.put(newDoc);
	rq.setResult(newDoc.getRevValue());



}

Value MarketControl::getMarketStatus()  {

	Value err = ordersDb.get("error",CouchDB::flgNullIfMissing);
	if (err == nullptr) {
		return Object("marketStatus","ok");
	} else {
		return Object("marketStatus","stopped")
				("reason",err);
	}

}


Value MarketControl::initRpc(RpcServer& rpcServer) {

	PMarketControl me = this;
	rpcServer.add("Order.create", me, &MarketControl::rpcOrderCreate);
	rpcServer.add("Order.modify", me, &MarketControl::rpcOrderUpdate);
	rpcServer.add("Order.cancel", me, &MarketControl::rpcOrderCancel);
	rpcServer.add("Order.get", me, &MarketControl::rpcOrderGet);
	rpcServer.add("Stream.orders",  me, &MarketControl::rpcStreamOrders);
	rpcServer.add("Stream.trades", me, &MarketControl::rpcStreamTrades);
	rpcServer.add("Stream.orderbook", me, &MarketControl::rpcStreamOrderbook);
	rpcServer.add("Stream.positions", me, &MarketControl::rpcStreamPositions);
	rpcServer.add("Stream.lastId", me, &MarketControl::rpcStreamLastId);
	rpcServer.add("Status.get", me, &MarketControl::rpcStatusGet);
	rpcServer.add("Status.clear", me, &MarketControl::rpcStatusClear);
	rpcServer.add("Orderbook.get",me, &MarketControl::rpcOrderbookGet);
	rpcServer.add("Config.get",me, &MarketControl::rpcConfigGet);
	rpcServer.add("Config.set",me, &MarketControl::rpcConfigSet);
	rpcServer.add("Chart.get",me, &MarketControl::rpcChartGet);

	return getMarketStatus();

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
	BasicFeed(couchit::CouchDB &db,
			  Value since,
			  RpcRequest rq,
			  String streamName)
				:FeedControl(db,since),rq(rq),streamName(streamName) {

	}
	virtual void init() override {
	}
	~BasicFeed() {
		stop();
	}

protected:
	RpcRequest rq;
	String streamName;
};

class MarketControl::OrderFeed: public BasicFeed {
public:
	using BasicFeed::BasicFeed;
	virtual void init() override {
		static couchit::View initView("_design/index/_view/queue",couchit::View::includeDocs|couchit::View::update);
		initialView = &initView;
		feed.setFilter(couchit::Filter("index/stream",couchit::Filter::includeDocs));
	}
	virtual void onEvent(Value seqNum, Value doc) override {
		rq.sendNotify(streamName,{seqNum.toString(),
				Object(doc)
				("id",doc["_id"])
				("_id",undefined)});
	}

};

class MarketControl::TradesFeed: public BasicFeed {
public:
	using BasicFeed::BasicFeed;
	virtual void init() override {
		feed.setFilter(couchit::Filter("trades/stream",couchit::Filter::includeDocs));
	}
	virtual void onEvent(Value seqNum, Value doc) override {
		rq.sendNotify(streamName,{seqNum.toString(),
				Object(doc)
				("id",doc["_id"])
				("_id",undefined)
				("_rev",undefined)
		});
	}

};

class MarketControl::PosFeed: public BasicFeed {
public:
	using BasicFeed::BasicFeed;
	virtual void init() override {
		feed.setFilter(couchit::Filter("positions/stream",couchit::Filter::includeDocs));
	}
	virtual void onEvent(Value seqNum, Value doc) override {
		rq.sendNotify(streamName,{seqNum.toString(),
				Object(doc)
				("user",doc["_id"].getString().substr(2))
				("_id",undefined)
				("_rev",undefined)
		});
	}
};

class MarketControl::OrderbookFeed: public BasicFeed {
public:
	using BasicFeed::BasicFeed;
	virtual void init() override {
		static couchit::View initView("_design/index/_view/orderbook",couchit::View::includeDocs|couchit::View::update);
		this->initialView = &initView;
		feed.setFilter(couchit::Filter("index/orderbook",couchit::Filter::includeDocs));
	}
	virtual void onEvent(Value seqNum, Value doc) override {
		sendNotify(rq, streamName, doc, seqNum.toString());
	}
	static void sendNotify(RpcRequest &rq, StrViewA streamName, Value doc, Value seq) {
		Object ntf;
		bool finished = doc["finished"].getBool();
		rq.sendNotify(streamName,{seq,{doc["_id"],doc["dir"],doc["limitPrice"],finished?Value(0):doc["size"]}});
	}

};

void MarketControl::rpcStreamOrders(RpcRequest rq) {
	static Value turnOffArgs = Value(json::array,{false});
	static Value turnOnArgs = {true,{"string","optional"} };
	if (rq.checkArgs(turnOffArgs)) {

		ordersFeed = nullptr;
		rq.setResult(true);


	} else if (rq.checkArgs(turnOnArgs)) {
		Value since = rq.getArgs()[1];
		ordersFeed = new OrderFeed(ordersDb, since, rq, "order");
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

		tradesFeed = nullptr;
		rq.setResult(true);


	} else if (rq.checkArgs(turnOnArgs)) {
		Value since = rq.getArgs()[1];
		tradesFeed = new TradesFeed(tradesDb, since, rq, "trade");
		tradesFeed->start();
		rq.setResult(true);


	} else {
		rq.setArgError();
	}
}

void MarketControl::rpcStreamPositions(RpcRequest rq) {
	static Value turnOffArgs = Value(json::array,{false});
	static Value turnOnArgs = {true,{"string","optional"} };
	if (rq.checkArgs(turnOffArgs)) {

		posFeed = nullptr;
		rq.setResult(true);


	} else if (rq.checkArgs(turnOnArgs)) {
		Value since = rq.getArgs()[1];
		posFeed = new PosFeed(posDb, since, rq, "position");
		posFeed->start();
		rq.setResult(true);


	} else {
		rq.setArgError();
	}
}

void MarketControl::rpcStreamOrderbook(RpcRequest rq) {
	static Value turnOffArgs = Value(json::array,{false});
	static Value turnOnArgs = {true,{"string","optional"} };
	if (rq.checkArgs(turnOffArgs)) {

		orderbookFeed = nullptr;
		rq.setResult(true);


	} else if (rq.checkArgs(turnOnArgs)) {
		Value since = rq.getArgs()[1];
		orderbookFeed = new OrderbookFeed(ordersDb, since, rq, "orderbook");
		orderbookFeed->start();
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
	using namespace couchit;
	std::condition_variable initWaitCond;
	std::mutex lock;

	thr = std::thread([&]{

		init();

		{
		std::unique_lock<std::mutex> _(lock);
		initWait = true;
		initWaitCond.notify_all();
		}

		Value lastDoc;
		if (!since.defined() && initialView != nullptr)
		{
			Query q = db.createQuery(*initialView);
			Result r = q.exec();
			for (Row rw : r) {
				onEvent(r.getUpdateSeq(),rw.doc);
				lastDoc = rw.doc;
			}
			since = r.getUpdateSeq();
		}

		if (since.defined())
			feed.since(since);
		try {
			feed >> [&](ChangedDoc x) {
				if (x.doc == lastDoc) return true;
				lastDoc = json::undefined;
				onEvent(x.seqId, x.doc);
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
	:feed(db.createChangesFeed()),  since(since), db(db), stopped(true)
{
	feed.setTimeout(-1);
	feed.includeDocs(true);
}

void MarketControl::rpcStreamLastId(RpcRequest rq) {
	if (!rq.checkArgs(json::array)) return rq.setArgError();

	rq.setResult(Object("orders",ordersDb.getLastSeqNumber())
					   ("trades",tradesDb.getLastSeqNumber())
					   ("positions",posDb.getLastSeqNumber())
			);

}

void quark::MarketControl::rpcStatusGet(RpcRequest rq) {
	rq.setResult(getMarketStatus());
}

void quark::MarketControl::rpcStatusClear(RpcRequest rq) {
	couchit::Document doc = ordersDb.get("error");
	doc.setDeleted();
	ordersDb.put(doc);
	rq.setResult(getMarketStatus());
}

void quark::MarketControl::rpcOrderbookGet(RpcRequest rq) {
	couchit::View orderbookView("_design/index/_view/orderbook",couchit::View::update);
	couchit::Result res = ordersDb.createQuery(orderbookView).exec();
	Array out;
	for (couchit::Row rw : res) {
		out.push_back({rw["id"],rw.key[0], rw.key[1], rw.value});
	}
	rq.setResult(out);
}


void MarketControl::rpcChartGet(RpcRequest rq) {

	static Value args = Value(json::array,{
			Object("startTime","integer")
			      ("endTime",{"integer","optional"})
			      ("timeFrame","string")
	});

	if (!rq.checkArgs(args)) return rq.setArgError();

	int groupLevel;
	int aggreg;
	Value arg = rq.getArgs()[0];
	Value startTime = arg["startTime"];
	Value endTime = arg["endTime"];
	Value timeFrame = arg["timeFrame"];

	StrViewA tfs = timeFrame.getString();
	if (tfs == "1m") {groupLevel=4; aggreg = 1;}
	else if (tfs == "3m") {groupLevel=4; aggreg = 3;}
	else if (tfs == "5m") {groupLevel=3; aggreg = 5;}
	else if (tfs == "10m") {groupLevel=3; aggreg = 10;}
	else if (tfs == "15m") {groupLevel=3; aggreg = 15;}
	else if (tfs == "30m") {groupLevel=3; aggreg = 30;}
	else if (tfs == "45m") {groupLevel=3; aggreg = 45;}
	else if (tfs == "1h") {groupLevel=2; aggreg = 60;}
	else if (tfs == "2h") {groupLevel=2; aggreg = 120;}
	else if (tfs == "3h") {groupLevel=2; aggreg = 180;}
	else if (tfs == "4h") {groupLevel=2; aggreg = 240;}
	else if (tfs == "6h") {groupLevel=2; aggreg = 360;}
	else if (tfs == "12h") {groupLevel=2; aggreg = 720;}
	else if (tfs == "1D") {groupLevel=1; aggreg = 1440;}
	else if (tfs == "2D") {groupLevel=1; aggreg = 2880;}
	else if (tfs == "3D") {groupLevel=1; aggreg = 4320;}
	else if (tfs == "7D") {groupLevel=1; aggreg = 10080;}
	else if (tfs == "1W") {groupLevel=1; aggreg = 10080;}
	else return rq.setError(400,"Undefined timeFrame",timeFrame);

	aggreg*=60;

	couchit::View chartView("_design/trades/_view/chart", couchit::View::update);
	auto query = tradesDb.createQuery(chartView);
	query.groupLevel(groupLevel);
	query.range(startTime, endTime,0);
	couchit::Result res = query.exec();

	Array result;
	result.push_back({groupLevel, aggreg});
	std::vector<Value> agrRecs;
	std::size_t agrtime = 0;

	auto calcTime = [](Value tmRec) {
		std::uintptr_t x = tmRec[0].getUInt() * 86400UL
				+ tmRec[1].getUInt() * 3600UL
				+ tmRec[2].getUInt() * 900UL
				+ tmRec[3].getUInt() * 60UL;
		return x;
	};

	auto doAggregate = [&]() -> json::Value {

		if (agrRecs.empty()) return json::undefined;
		if (agrRecs.size() == 1) return agrRecs[0].replace("t", agrtime*aggreg);
		double h = agrRecs[0]["h"].getNumber();
		double l = agrRecs[0]["l"].getNumber();
		double o = agrRecs[0]["o"].getNumber();
		double c = agrRecs[0]["c"].getNumber();
		double v = agrRecs[0]["v"].getNumber();
		std::size_t n = agrRecs[0]["n"].getUInt();
		double s = agrRecs[0]["s"].getNumber();
		double s2 = agrRecs[0]["s2"].getNumber();
		double v2 = agrRecs[0]["v2"].getNumber();
		for (std::size_t i = 2, cnt = agrRecs.size(); i < cnt; i++) {
			double xh = agrRecs[i]["h"].getNumber();
			double xl = agrRecs[i]["l"].getNumber();
			double xo = agrRecs[i]["o"].getNumber();
			double xc = agrRecs[i]["c"].getNumber();
			double xv = agrRecs[i]["v"].getNumber();
			std::size_t xn = agrRecs[i]["n"].getUInt();
			double xs = agrRecs[i]["s"].getNumber();
			double xs2 = agrRecs[i]["s2"].getNumber();
			double xv2 = agrRecs[i]["v2"].getNumber();
			if (xh > h) h = xh;
			if (xl < l) l = xl;
			c = xc;
			v += xv;
			n += xn;
			s += xs;
			s2 += xs2;
			v2 += xv2;
		}
		return Value(Object("o",o)
				("h",h)
				("l",l)
				("c",c)
				("o",o)
				("v",v)
				("n",n)
				("s",s)
				("s2",s2)
				("t", agrtime*aggreg)
				("v2",v2));
	};

	for (couchit::Row rw : res) {
		std::size_t tm = calcTime(rw.key);
		std::size_t atm = tm / aggreg;
//		std::cerr << tm << "," << atm << std::endl;
		if (atm != agrtime) {
			result.push_back(doAggregate());
			agrRecs.clear();
			agrtime = atm;
		}
		agrRecs.push_back(rw.value);
	}
	result.push_back(doAggregate());

	rq.setResult(result);

}


}

/* namespace quark */


