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
	rpcServer.add("Trades.chart",me, &MarketControl::rpcChartGet);
	rpcServer.add("Trades.stats",me, &MarketControl::rpcTradesStats);

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

void MarketControl::rpcStatusGet(RpcRequest rq) {
	rq.setResult(getMarketStatus());
}

void MarketControl::rpcStatusClear(RpcRequest rq) {
	couchit::Document doc = ordersDb.get("error");
	doc.setDeleted();
	ordersDb.put(doc);
	rq.setResult(getMarketStatus());
}

void MarketControl::rpcOrderbookGet(RpcRequest rq) {
	couchit::View orderbookView("_design/index/_view/orderbook",couchit::View::update);
	couchit::Result res = ordersDb.createQuery(orderbookView).exec();
	Array out;
	for (couchit::Row rw : res) {
		out.push_back({rw["id"],rw.key[0], rw.key[1], rw.value});
	}
	rq.setResult(out);
}


static Value crackTime(Value tm) {
    if (tm.defined()) {
	std::uintptr_t timestamp = tm.getUInt();
        return {(timestamp/604800),(timestamp/86400)%7, (timestamp/14400)%6, (timestamp/3600)%4, (timestamp/900)%4,(timestamp/300)%3, (timestamp/60)%5};
    } else {
	return tm;
    }
}

static std::uintptr_t decodeTime(Value tmRec) {
		std::uintptr_t x =
				  tmRec[0].getUInt() * 604800UL
				+ tmRec[1].getUInt() * 86400UL
				+ tmRec[2].getUInt() * 14400UL
				+ tmRec[3].getUInt() * 3600UL
				+ tmRec[4].getUInt() * 900UL
				+ tmRec[5].getUInt() * 300UL
				+ tmRec[6].getUInt() * 60UL;
		return x;
	};

static double fixFloat(double c) {
    return floor(c * 1000000000+0.5)/ 1000000000;
}

struct TimeFrameDef {
	StrViewA name;
	unsigned int groupLevel;
	unsigned int divisor;
};

static TimeFrameDef supportedFrames[] = {

		{"1m",7,1},
		{"2m",7,2},
		{"3m",7,3},
		{"5m",6,5},
		{"10m",6,10},
		{"15m",5,15},
		{"30m",5,30},
		{"45m",5,45},
		{"1h",4,60},
		{"2h",4,120},
		{"3h",4,180},
		{"4h",3,240},
		{"6h",4,360},
		{"8h",3,480},
		{"12h",3,720},
		{"1D",2,1440},
		{"2D",2,2880},
		{"3D",2,4320},
		{"7D",1,10080},
		{"1W",1,10080},
		{"14D",1,20160},
		{"2W",1,20160}
};


void MarketControl::rpcChartGet(RpcRequest rq) {


	static Value args = Value(json::array,{
			Object("startTime","integer")
			      ("endTime",{"integer","optional"})
			      ("timeFrame",{"string","integer"})
	});

	if (!rq.checkArgs(args)) return rq.setArgError();

	const TimeFrameDef *selTmf = nullptr;


	Value arg = rq.getArgs()[0];
	Value startTime = arg["startTime"];
	Value endTime = arg["endTime"];
	Value timeFrame = arg["timeFrame"];

	if (timeFrame.type() == json::number) {
		auto d = timeFrame.getUInt();
		for (auto &&x : supportedFrames) {
			if (x.divisor == d) {
				selTmf = &x;
				break;
			}
		}
	} else {
		auto d = timeFrame.getString();
		for (auto &&x : supportedFrames) {
			if (x.name == d) {
				selTmf = &x;
				break;
			}
		}
	}


	if (selTmf == nullptr) {

		Array list;
		for (auto &&x : supportedFrames) {
			list.push_back(x.name);
		}
		for (auto &&x : supportedFrames) {
			list.push_back(x.divisor);
		}

		rq.setError(400, "Unknown timeFrane", list);
		return ;

	}

	unsigned int aggreg=60*selTmf->divisor;;

	couchit::View chartView("_design/trades/_view/chart", couchit::View::update);
	auto query = tradesDb.createQuery(chartView);
	query.groupLevel(selTmf->groupLevel);
	query.range(crackTime(startTime), crackTime(endTime),0);
	couchit::Result res = query.exec();

	Array jres;

	auto iter = res.begin();
	if (iter != res.end()) {

		Value rw = *iter;
		ChartData agr;
		agr.fromDB(rw, aggreg);
		++iter;
		while (iter != res.end()) {
			ChartData item;
			item.fromDB(*iter, aggreg);
			if (item.time != agr.time) {
				jres.push_back(agr.toJson());
				agr = item;
			} else {
				agr.aggregate(item);
			}
			++iter;
		}
		jres.push_back(agr.toJson());
	}

	rq.setResult(jres);

}

void MarketControl::ChartData::fromDB(json::Value v, std::uintptr_t timeAgr) {

	Value key = v["key"];
	Value data = v["value"];

	time = (decodeTime(key)/timeAgr)*timeAgr;
	count = data[5].getUInt();
	close_index=open_index = data[9].getUInt();
	high = data[1].getNumber();
	low = data[2].getNumber();
	open = data[0].getNumber();
	close = data[3].getNumber();
	volume = data[4].getNumber();
	sum = data[6].getNumber();
	sum2 = data[7].getNumber();
	volume2 = data[8].getNumber();
}


json::Value MarketControl::ChartData::toJson() const {
	return Value(Object("open",open)
			("high",high)
			("low",low)
			("close",close)
			("volume",volume)
			("count",count)
			("sum",sum)
			("sum2",sum2)
			("time", time)
			("index",open_index)
			("volume2",volume2));
}

void MarketControl::ChartData::aggregate(const ChartData& with) {

	time = std::min(time, with.time);
	count += with.count;
	sum += with.sum;
	sum2 += with.sum2;
	volume += with.volume;
	volume2 += with.volume2;
	high = std::max(high, with.high);
	low = std::max(low, with.low);
	if (open_index > with.open_index) {
		open = with.open;
		open_index = with.open_index;
	} else if (close_index < with.close_index) {
		close = with.close;
		close_index = with.close_index;
	}

}

void MarketControl::rpcTradesStats(RpcRequest rq) {
	static Value args = Value(json::array,{
			Object("startTime","integer")
			      ("endTime",{"integer","optional"})
	});

	if (!rq.checkArgs(args)) return rq.setArgError();

	Value arg = rq.getArgs()[0];
	Value startTime = arg["startTime"];
	Value endTime = arg["endTime"];

	couchit::View chartView("_design/trades/_view/chart", couchit::View::update);
	auto query = tradesDb.createQuery(chartView);
	query.reduceAll();
	query.range(crackTime(startTime), crackTime(endTime),0);
	couchit::Result res = query.exec();

	ChartData d;
	d.fromDB(res[0],1);
	rq.setResult(d.toJson());


}


}
/* namespace quark */


