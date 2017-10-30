/*
 * MarketControl.cpp
 *
 *  Created on: Jun 11, 2017
 *      Author: ondra
 */

#include "marketControl.h"

#include <imtjson/fnv.h>


#include <couchit/changeset.h>

#include <couchit/couchDB.h>
#include <couchit/document.h>
#include <couchit/exception.h>
#include <couchit/query.h>
#include <imtjson/rpc.h>
#include <imtjson/stringview.h>

#include "../common/config.h"
#include "../quark_lib/order.h"

namespace quark {

MarketControl::MarketControl(Value cfg, String dbname)
	:ordersDb(initCouchDBConfig(cfg,dbname,"-orders"))
	,tradesDb(initCouchDBConfig(cfg,dbname,"-trades"))
	,posDb(initCouchDBConfig(cfg,dbname,"-positions"))
	,orderControl(ordersDb)
	,marketId(dbname)
{


}

static json::Value removeServiceMembers(json::Value doc) {
	return 	Object(doc)
			("_id",undefined)
			("_rev",undefined);
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
			   "\"%\":{\"any\",\"optional\"}"
			"},\"string\"]");

	if (!rq.checkArgs(args)) return rq.setArgError();
	couchit::Document newDoc;
	newDoc.setBaseObject(rq.getArgs()[0]);
	newDoc.setID("settings");
	newDoc.setRev(rq.getArgs()[1]);
	ordersDb.put(newDoc);
	rq.setResult(newDoc.getRevValue());
}

void MarketControl::rpcConfigUpdateFrom(RpcRequest rq) {
	static Value args(json::array,{"string"});
	if (!rq.checkArgs(args)) return rq.setArgError();
	couchit::Document cfg = ordersDb.get("settings");
	cfg.set("updateUrl", rq.getArgs()[0]);
	ordersDb.put(cfg);
	rq.setResult(cfg.getRevValue());
}


Value MarketControl::getMarketStatus()  {

	Value err = ordersDb.get("error",CouchDB::flgNullIfMissing);
	return getMarketStatus(err);

}
Value MarketControl::getMarketStatus(Value err)  {
	if (err == nullptr) {
		return Object("marketStatus","ok");
	} else {
		return Object("marketStatus","stopped")
				("reason",removeServiceMembers(err));
	}

}


Value MarketControl::initRpc(RpcServer& rpcServer) {

	PMarketControl me = this;
	rpcServer.add("Order.create", me, &MarketControl::rpcOrderCreate);
	rpcServer.add("Order.modify", me, &MarketControl::rpcOrderUpdate);
	rpcServer.add("Order.cancel", me, &MarketControl::rpcOrderCancel);
	rpcServer.add("Order.cancelAll", me, &MarketControl::rpcOrderCancelAll);
	rpcServer.add("Order.get", me, &MarketControl::rpcOrderGet);
	rpcServer.add("Stream.orders",  me, &MarketControl::rpcStreamOrders);
	rpcServer.add("Stream.trades", me, &MarketControl::rpcStreamTrades);
	rpcServer.add("Stream.orderbook", me, &MarketControl::rpcStreamOrderbook);
	rpcServer.add("Stream.positions", me, &MarketControl::rpcStreamPositions);
	rpcServer.add("Stream.lastId", me, &MarketControl::rpcStreamLastId);
	rpcServer.add("Stream.status", me, &MarketControl::rpcStreamStatus);
	rpcServer.add("Status.get", me, &MarketControl::rpcStatusGet);
	rpcServer.add("Status.clear", me, &MarketControl::rpcStatusClear);
	rpcServer.add("Orderbook.get",me, &MarketControl::rpcOrderbookGet);
	rpcServer.add("Config.get",me, &MarketControl::rpcConfigGet);
	rpcServer.add("Config.set",me, &MarketControl::rpcConfigSet);
	rpcServer.add("Config.updateFrom",me, &MarketControl::rpcConfigUpdateFrom);
	rpcServer.add("Chart.get",me, &MarketControl::rpcChartGet);
	rpcServer.add("Trades.chart",me, &MarketControl::rpcChartGet);
	rpcServer.add("Trades.stats",me, &MarketControl::rpcTradesStats);
	rpcServer.add("User.orders",me, &MarketControl::rpcUserOrders);
	rpcServer.add("User.trades",me, &MarketControl::rpcUserTrades);
	rpcServer.add("Control.stop",me, &MarketControl::rpcControlStop);
	rpcServer.add("Control.ping",me, &MarketControl::rpcControlPing);
	rpcServer.add("Control.dumpState",me, &MarketControl::rpcControlDumpState);
	rpcServer.add("Data.deleteOld",me, &MarketControl::rpcPurgeFunction);
	rpcServer.add("Data.replication",me, &MarketControl::rpcReplication);

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
	} catch (ConflictError &e) {
		rq.setError(409,"Conflict",e.getActualDoc());
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
			  String streamName,
			  Value filter)
				:FeedControl(db,since,filter),rq(rq),streamName(streamName) {

	}
	virtual void init() override {
	}

	void streamError(Value x) {
		rq.sendNotify(streamName, {nullptr,Object("error", x)});
	}

	virtual void onError() override {
		try {
			throw;
		} catch (std::exception &e) {
			streamError(e.what());
		} catch (...) {
			streamError("unspecified error");
		}
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
		//do not show control orders
		if (doc["type"].getString() == "control") return;
		if (filter.defined() && doc["user"]!=filter["user"]) return;
		rq.sendNotify(streamName,{seqNum.toString(),OrderControl::normFields(doc)});
	}

};

static json::Value normTrade(json::Value doc) {
	return 	Object(doc)
			("id",doc["_id"])
			("_id",undefined)
			("_rev",undefined);
}


class MarketControl::TradesFeed: public BasicFeed {
public:
	using BasicFeed::BasicFeed;
	virtual void init() override {
		feed.setFilter(couchit::Filter("trades/stream",couchit::Filter::includeDocs));
	}
	virtual void onEvent(Value seqNum, Value doc) override {
		if (filter.defined() && doc["sellUser"]!=filter["user"] && doc["buyUser"]!=filter["user"]) return;
		rq.sendNotify(streamName,{seqNum.toString(),normTrade(doc)});
	}

};

class MarketControl::PosFeed: public BasicFeed {
public:
	using BasicFeed::BasicFeed;
	virtual void init() override {
		feed.setFilter(couchit::Filter("positions/stream",couchit::Filter::includeDocs));
	}
	virtual void onEvent(Value seqNum, Value doc) override {
		if (filter.defined() && doc["_id"].getString().substr(2)!=filter["user"].getString()) return;
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


class MarketControl::StatusFeed: public BasicFeed {
public:
	using BasicFeed::BasicFeed;
	virtual void init() override {
		feed.forDocs({"error","warning"});
	}
	virtual void onEvent(Value seqNum, Value doc) override {
		sendNotify(rq, streamName, doc, seqNum.toString());
	}
	static void sendNotify(RpcRequest &rq, StrViewA streamName, Value doc, Value seq) {
		Value out;
		if (doc.isNull()) {
			Object v;
			v.set("type","status_update");
			v.set("ok", true);
			out = v;
		}
		else if (doc["_id"] == "error") {
			Object v;
			v.set("type","status_update");
			if (doc["_deleted"].getBool()) {
				v.set("ok", true);
			} else {
				v.set("ok", false);
				v.set("reason",removeServiceMembers(doc));
			}
			out = v;

		} else if (doc["_id"] == "warning") {
			out = removeServiceMembers(doc);
		}

		rq.sendNotify(streamName, {seq, out});
	}

};

static Value stream_turnOffArgs = Value(json::array,{false});
static Value stream_turnOnArgs = {true,{"string","optional"},{Object("user",{"any","optional"}),"optional"} };
static Value stream_turnOnSimple = Value(json::array,{true});


void MarketControl::rpcStreamOrders(RpcRequest rq) {
	if (rq.checkArgs(stream_turnOffArgs)) {

		ordersFeed = nullptr;
		rq.setResult(true);


	} else if (rq.checkArgs(stream_turnOnArgs)) {
		Value args = rq.getArgs();
		Value since = args[1];
		Value filter;
		if (args.size()>2) filter = args[2];
		ordersFeed = new OrderFeed(ordersDb, since, rq, "order", filter);
		ordersFeed->start();
		rq.setResult(true);


	} else {
		rq.setArgError();
	}
}

void MarketControl::rpcStreamTrades(RpcRequest rq) {
	if (rq.checkArgs(stream_turnOffArgs)) {

		tradesFeed = nullptr;
		rq.setResult(true);


	} else if (rq.checkArgs(stream_turnOnArgs)) {
		Value args = rq.getArgs();
		Value since = args[1];
		Value filter;
		if (args.size()>2) filter = args[2];
		tradesFeed = new TradesFeed(tradesDb, since, rq, "trade", filter);
		tradesFeed->start();
		rq.setResult(true);


	} else {
		rq.setArgError();
	}
}

void MarketControl::rpcStreamPositions(RpcRequest rq) {
	if (rq.checkArgs(stream_turnOffArgs)) {

		posFeed = nullptr;
		rq.setResult(true);


	} else if (rq.checkArgs(stream_turnOnArgs)) {
		Value args = rq.getArgs();
		Value since = args[1];
		Value filter;
		if (args.size()>2) filter = args[2];
		posFeed = new PosFeed(posDb, since, rq, "position", filter);
		posFeed->start();
		rq.setResult(true);


	} else {
		rq.setArgError();
	}
}

void MarketControl::rpcStreamStatus(RpcRequest rq) {
	if (rq.checkArgs(stream_turnOffArgs)) {

		statusFeed = nullptr;
		rq.setResult(true);


	} else if (rq.checkArgs(stream_turnOnSimple)) {

		couchit::Query q = ordersDb.createQuery(View::includeDocs);
		couchit::Result r = q.key("error").exec();
		Value errdoc = nullptr;
		if (!r.empty()) {
			errdoc = couchit::Row(r[0]).doc;
		}
		rq.setResult(true);
		Value since = r.getUpdateSeq();
		StatusFeed::sendNotify(rq,"status",errdoc,since);

		statusFeed = new StatusFeed(ordersDb, since, rq, "status", Value());
		statusFeed->start();



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
		orderbookFeed = new OrderbookFeed(ordersDb, since, rq, "orderbook",json::undefined);
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

		try {
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

			feed >> [&](ChangedDoc x) {
				if (x.doc == lastDoc) return true;
				lastDoc = json::undefined;
				onEvent(x.seqId, x.doc);
				return true;
			};
		} catch (...) {
			onError();
		}
	});

	std::unique_lock<std::mutex> _(lock);
	initWaitCond.wait(_,[&]{return initWait;});
	stopped = false;
}

MarketControl::FeedControl::FeedControl(CouchDB& db, Value since, Value filter)
	:feed(db.createChangesFeed()),  since(since), db(db), stopped(true), filter(filter)
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
	open_index = String(data[9]);
	close_index = String(data[10]);
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
			("firstTrade",open_index)
			("lastTrade",close_index)
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

static	couchit::View ordersByUser("_design/index/_view/users", couchit::View::update);
static	couchit::View tradesByUser("_design/trades/_view/byUser", couchit::View::update);


static Value getUserDataArgs = Value(json::array,{
		Object("user","any")
		      ("startTime","integer")
		      ("endTime",{"integer","optional"})
});



static void setupUserQuery(couchit::Query &q, Value args) {

	Value user = args["user"];
	Value startTime = args["startTime"];
	Value endTime =args["endTime"];

	couchit::Value startKey = {user,startTime};
	couchit::Value endKey = {user, endTime.defined()?endTime:Value(json::object)};

	q.range(startKey,endKey);

}

void MarketControl::rpcUserOrders(RpcRequest rq) {
	if (!rq.checkArgs(getUserDataArgs)) return rq.setArgError();

	auto query = ordersDb.createQuery(ordersByUser);
	query.includeDocs();
	setupUserQuery(query,rq.getArgs()[0]);


	couchit::Result res = query.exec();
	Array rx;
	rx.reserve(res.size());
	for (couchit::Row r: res) {
		rx.push_back(OrderControl::normFields(r.doc));
	}
	rq.setResult(rx);

}

void MarketControl::rpcUserTrades(RpcRequest rq) {
	if (!rq.checkArgs(getUserDataArgs)) return rq.setArgError();

		auto query = tradesDb.createQuery(tradesByUser);
		query.includeDocs();
		setupUserQuery(query,rq.getArgs()[0]);


		couchit::Result res = query.exec();
		Array rx;
		rx.reserve(res.size());
		for (couchit::Row r: res) {
			rx.push_back(normTrade(r.doc));
		}
		rq.setResult(rx);

}

void MarketControl::rpcControlStop(RpcRequest rq) {
	callDaemonService("stop",rq.getArgs(),rq);
}

void MarketControl::rpcControlPing(RpcRequest rq) {
	callDaemonService("ping",rq.getArgs(),rq);
}

void MarketControl::rpcOrderCancelAll(RpcRequest rq) {
	couchit::View userActive("_design/index/_view/active", View::includeDocs|View::update);
	unsigned int count = 0;
	do {
		couchit::Query q = ordersDb.createQuery(userActive);
		if (!rq.getArgs().empty()) q.keys(rq.getArgs());
		q.limit(2000); //cancel max 2000 orders per one cycle

		couchit::Result res = q.exec();
		if (res.empty()) {
			rq.setResult(Object("count", count)("success",true));
			return;
		}

		couchit::Changeset chs = ordersDb.createChangeset();
		for (couchit::Row rw : res) {

			couchit::Document doc(rw.doc);
			doc(OrderFields::cancelReq,true);
			chs.update(doc);

		}
		try {
			chs.commit();
			count += res.size();
		} catch (couchit::UpdateException &e) {
			//if there is conflict, we will try again
			count += res.size() - e.getErrorCnt();
		}
	} while (true);

}

void MarketControl::callDaemonService(String command,
		Value params, RpcRequest req) {


	couchit::Filter docwait("index/waitfordoc",View::includeDocs);

	couchit::Document doc = ordersDb.newDocument("o.");
	Value id = doc.getIDValue();
	doc(OrderFields::type, "control");
	doc(OrderFields::status, Status::strValidating);
	doc("request", Object("id",req.getId())
						("jsonrpc","2.0")
						("method",command)
						("params",params));


	ordersDb.put(doc);
	req.sendNotify("control",Object("command",command)("params",params)("id",req.getId())("order", doc.getID()));
	couchit::Query q = ordersDb.createQuery(couchit::View::includeDocs);
	q.key(id);
	couchit::Result r = q.exec();
	couchit::SeqNumber ofs = r.getUpdateSeq();
	couchit::Row rw = r[0];
	doc = rw.doc;
	while (doc[OrderFields::finished].getBool() == false) {
		couchit::ChangesFeed chf = ordersDb.createChangesFeed();
		Value chg = chf.since(ofs).includeDocs(true).setFilter(docwait).arg("doc",id).setTimeout(30000).exec().getAllChanges();
		if (chg.empty()) {
			doc.setDeleted();
			try {
				ordersDb.put(doc);
				req.setError(502,"Service timeout");
				return ;
			} catch (couchit::UpdateException &) {
				continue;
			}
		}
		couchit::ChangedDoc chdoc = chg[0];
		doc = chdoc.doc;
		ofs = chf.getLastSeq();
	}
	if (doc[OrderFields::status] == Status::strRejected) {
		req.setError(403,"Service rejected the request", doc[OrderFields::error]);
	} else if (doc[OrderFields::status] == Status::strExecuted) {
		Value resp = doc["response"];
		Value result = resp["result"];
		Value error = resp["error"];
		if (result.defined() && !result.isNull()) {
			req.setResult(result);
		} else {
			req.setError(error);
		}
	} else {
		req.setError(500,"Unknown response", doc);
	}
	try {
		doc.setDeleted();
		ordersDb.put(doc);
	} catch (couchit::UpdateException &) {
	}


}


void MarketControl::rpcControlDumpState(RpcRequest rq) {
	callDaemonService("dumpState",rq.getArgs(),rq);
}

template<typename Filter>
static size_t runPurge2(couchit::CouchDB &admin,
		const couchit::View &viewRefresh, const Filter &filter) {

	using namespace couchit;

	bool rep;

	size_t cnt = 0;

	do {
		ChangesFeed feed = admin.createChangesFeed();
		Changes chs = feed.limit(1000).includeDocs().exec();

		Object docs;
		Array revs;
		for (ChangedDoc chdoc : chs) {

			if (filter(chdoc)) {

				revs.clear();
				for (Value x : chdoc.revisions) {
					revs.push_back(x["rev"]);
				}

				docs(chdoc.id,revs);
				cnt++;
			}
		}

		rep = docs.size() != 0;
		if (rep) {
			auto conn = admin.getConnection("_purge");
			bool tryagain = false;
			do {
				try {
					admin.requestPOST(conn, docs, nullptr, 0);
					tryagain = false;
				} catch (couchit::RequestError &r) {
					if (r.getCode() == 500 && r.getExtraInfo()["reason"].getString() == "purge_during_compaction") {
						std::this_thread::sleep_for(std::chrono::seconds(2));
						tryagain = true;
					} else {
						throw;
					}
				}
			} while (tryagain);
			admin.updateView(viewRefresh,true);
		}

	} while (rep);
	return cnt;

}

template<typename Filter>
static size_t runPurge(const couchit::Config &cfg,
		const couchit::View &viewRefres, const Filter &filter) {


	CouchDB admin(cfg);
	auto conn = admin.getConnection("/");
	Value info = admin.requestGET(conn, nullptr, 0);
	String version ( info["version"]);
	if (version.substr(0,2) == "2.") {
		throw std::runtime_error("Not implemented yet");
	} else if (version.substr(0,2) == "1.") {

		return runPurge2(admin, viewRefres,filter);


	} else {
		throw std::runtime_error("Unsupported database");
	}


}



void MarketControl::rpcPurgeFunction(RpcRequest rq) {

	static couchit::View ordersRefresh("_design/index/_view/queue");
	static couchit::View tradesRefresh("_design/trades/_view/chart");


	static Value argscheck = Value::fromString(
			"[{"
			   "\"timestamp\": \"integer\","
			   "\"login\": \"string\","
			   "\"password\": \"string\""
			"}]");
	if (!rq.checkArgs(argscheck)) return rq.setArgError();
	Value args = rq.getArgs();
	size_t timestamp = args[0]["timestamp"].getUInt();
	String login ( args[0]["login"]);
	String password ( args[0]["password"]);

	couchit::Config ordersCfg =  ordersDb.getConfig();
	ordersCfg.authInfo.username = login;
	ordersCfg.authInfo.password = password;
	couchit::Config tradesCfg =  tradesDb.getConfig();
	tradesCfg.authInfo.username = login;
	tradesCfg.authInfo.password = password;

	std::size_t totOrders = couchit::Result(ordersDb.createQuery(0).limit(0).exec()).getTotal();
	std::size_t totTrades = couchit::Result(tradesDb.createQuery(0).limit(0).exec()).getTotal();

	size_t cntOrders = runPurge(ordersCfg, ordersRefresh,
			[&](const couchit::ChangedDoc &doc){
		return doc.id.substr(0,2) == "o." && (doc.doc["finished"].getBool() == true || doc.deleted)
				&& doc.doc["timeModified"].getUInt() < timestamp;
	});
	size_t cntTrades = runPurge(tradesCfg, tradesRefresh,
			[&](const couchit::ChangedDoc &doc){
		Value tm = doc.doc["time"];
		return tm.defined() && (doc.deleted || tm.getUInt() < timestamp);
	});

	rq.setResult(Object("orders",
					Object("total", totOrders)
					      ("deleted", cntOrders)
					      ("remain", totOrders - cntOrders))
				 	   ("trades",
							Object("total", totTrades)
							      ("deleted", cntTrades)
							      ("remain", totTrades - cntTrades))
					   );
}

static String getReplicationDocName(CouchDB &db, String url) {
	std::uint32_t hash = 0;
	{
		FNV1a32 hcalc(hash);
		for (auto c : StrViewA(url)) hcalc(c);
	};
	std::ostringstream buff;
	buff << db.getCurrentDB() << "_" << hash;
	return buff.str();
}

static String getReplicationTarget(CouchDB &db, const String &url, bool singleDB, const couchit::AuthInfo *auth) {
	std::ostringstream buff;
	auto spl = url.indexOf("://");
	buff << url.substr(0,spl+3);
	if (auth) buff << auth->username << ":" << auth->password << "@";
	buff << url.substr(spl+3);
	if (!singleDB) {
		buff << db.getCurrentDB();
	}
	return buff.str();

}

static void controlReplication(CouchDB &db, const couchit::Filter &filter, const String &url,
		const couchit::AuthInfo *auth, bool enable, bool singleDB, bool live) {

	String repName = getReplicationDocName(db, url);
	CouchDB repdb(db);
	repdb.setCurrentDB("_replicator");

	couchit::Document repDoc = repdb.get(repName,CouchDB::flgCreateNew);
	if (!enable) {
		if (!repDoc.getRevValue().defined()) return;
		repDoc.setDeleted(StringView<StrViewA>(), false);
		repdb.put(repDoc);
	} else {
		repDoc.setContent(Object
				("source",db.getCurrentDB())
				("target",getReplicationTarget(db,url, singleDB, auth))
				("continuous",true)
				("create_target",true)
				("filter",live?Value():Value(filter.viewPath))
				("user_ctx",Object("name",db.getConfig().authInfo.username)
								("roles",Value(json::array,{"quark_rpc"}))));
		repdb.put(repDoc);
	}
}

static String removePasswordFromUrl(StrViewA url) {
	auto p = url.indexOf("://")+3;
	auto q = url.indexOf("@",p);
	if (q != url.npos) {
		return String({url.substr(0,p), url.substr(q+1)});
	} else {
		return url;
	}
}

static Value getReplicationStatus(CouchDB &db, String url) {

	String repName = getReplicationDocName(db, url);
	CouchDB repdb(db);
	repdb.setCurrentDB("_replicator");
	Value repdoc = repdb.get(repName, CouchDB::flgNullIfMissing);
	if (repdoc.isNull()) return repdoc;

	Value state = repdoc["_replication_state"];
	Value outState;
	if (!state.defined()) outState ="starting";
	else if (state.getString() == "triggered") outState ="running";
	else outState = state;

	return Object("url", removePasswordFromUrl(repdoc["target"].getString()))
			     ("state",outState )
			     ("reason", repdoc["_replication_state_reason"])
			     ("time", repdoc["_replication_state_time"]);

}

void quark::MarketControl::rpcReplication(RpcRequest rq) {

	static Value argscheck = Value::fromString(
			"[{"
			   "\"enable\": [\"boolean\",\"optional\"],"
			   "\"url\": \"string\","
			   "\"auth\": [\"optional\",{\"username\":\"string\",\"password\":\"string\"}],"
			   "\"singleDB\":[\"optional\",\"boolean\"],"
			   "\"mode\":[\"'backup\",\"'live\",\"optional\"]"
			"}]");

	if (!rq.checkArgs(argscheck)) return rq.setArgError();
	Value args = rq.getArgs()[0];
	String url ( args["url"]);
	if (!args["enable"].defined()) {
		//get replication status
		Value orders = getReplicationStatus(ordersDb, url);
		Value pos = getReplicationStatus(posDb, url);
		Value trades = getReplicationStatus(tradesDb, url);
		rq.setResult(Object("orders",orders)
				("positions",pos)
				("trades",trades));
	} else {
		bool enable = args["enable"].getBool();
		bool singleDB = args["singleDB"].getBool();
		couchit::AuthInfo auth;
		Value authVal = args["auth"];
		bool hasAuth = authVal.defined();
		bool modeLive = args["mode"].getString() == "live";
		if (hasAuth) {
			auth.username = String(authVal["username"]);
			auth.password = String(authVal["password"]);
		}


		if (url.substr(0,7) != "http://" && url.substr(0,8) != "https://") {
			return rq.setError(400,"Url incorrect format", url);
		}
		const couchit::AuthInfo *authPtr = hasAuth?&auth:nullptr;
		controlReplication(ordersDb,  couchit::Filter("index/replication"), url, authPtr, enable, singleDB,modeLive);
		controlReplication(posDb, couchit::Filter("positions/replication"), url, authPtr, enable, singleDB,modeLive);
		controlReplication(tradesDb, couchit::Filter("trades/replication"), url, authPtr, enable, singleDB,modeLive);
		rq.setResult(true);
	}
}


}

/* namespace quark */


