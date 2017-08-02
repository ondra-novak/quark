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


	Value getMarketStatus();

	class FeedControl: public RefCntObj {
	public:
		couchit::ChangesFeed feed;
		std::thread thr;
		View initialView;
		Value since;
		CouchDB &db;
		bool stopped;

		FeedControl(CouchDB &db, Value since, View initialView);
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


	PFeedControl ordersFeed, tradesFeed, posFeed, orderbookFeed;
	OrderControl orderControl;









};

typedef RefCntPtr<MarketControl> PMarketControl;

} /* namespace quark */

