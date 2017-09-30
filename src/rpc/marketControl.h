#pragma once
#include <couchit/couchDB.h>
#include <couchit/changes.h>
#include <imtjson/refcnt.h>
#include <imtjson/rpc.h>
#include <imtjson/value.h>
#include <thread>

#include "orderControl.h"

namespace quark {
using couchit::CouchDB;
using couchit::View;

using namespace json;

class MarketControl: public RefCntObj {
public:
	MarketControl(Value cfg, StrViewA dbname);

	Value initRpc(RpcServer &rpcServer);
	bool testDatabase();

protected:

	CouchDB ordersDb;
	CouchDB tradesDb;
	CouchDB posDb;



	void rpcOrderCreate(RpcRequest rq);
	void rpcOrderUpdate(RpcRequest rq);
	void rpcOrderCancel(RpcRequest rq);
	void rpcOrderGet(RpcRequest rq);
	void rpcStreamOrders(RpcRequest rq);
	void rpcStreamOrderbook(RpcRequest rq);
	void rpcStreamTrades(RpcRequest rq);
	void rpcStreamPositions(RpcRequest rq);
	void rpcStreamLastId(RpcRequest rq);
	void rpcStatusGet(RpcRequest rq);
	void rpcStatusClear(RpcRequest rq);
	void rpcOrderbookGet(RpcRequest rq);
	void rpcConfigGet(RpcRequest rq);
	void rpcConfigSet(RpcRequest rq);
	void rpcChartGet(RpcRequest rq);
	void rpcTradesStats(RpcRequest rq);
	void rpcUserOrders(RpcRequest rq);
	void rpcUserTrades(RpcRequest rq);
	void rpcControlStop(RpcRequest rq);
	void rpcControlPing(RpcRequest rq);
	void rpcControlDumpState(RpcRequest rq);
	void rpcOrderCancelAll(RpcRequest rq);


	Value getMarketStatus();

	class FeedControl: public RefCntObj {
	public:
		couchit::ChangesFeed feed;
		std::thread thr;
		const View *initialView = nullptr;
		Value since;
		CouchDB &db;
		bool stopped;

		FeedControl(CouchDB &db, Value since);
		virtual ~FeedControl() {stop();}

		virtual void init() = 0;
		virtual void onEvent(Value seqNum, Value doc) = 0;
		void stop();
		void start();


	};

	class BasicFeed;
	class OrderFeed;
	class TradesFeed;
	class PosFeed;
	class OrderbookFeed;

	typedef RefCntPtr<FeedControl> PFeedControl;


	struct ChartData {
		std::uintptr_t time;
		std::uintptr_t count;
		String open_index;
		String close_index;
		double open;
		double close;
		double high;
		double low;
		double volume;
		double volume2;
		double sum;
		double sum2;

		void fromDB(json::Value v, std::uintptr_t timeAgr);
		json::Value toJson() const;

		void aggregate(const ChartData &with);

	};

	PFeedControl ordersFeed, tradesFeed, posFeed, orderbookFeed;
	OrderControl orderControl;

	void callDaemonService(String command, Value params, RpcRequest req);








};

typedef RefCntPtr<MarketControl> PMarketControl;

} /* namespace quark */

