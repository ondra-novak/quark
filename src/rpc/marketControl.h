#pragma once
#include <couchit/couchDB.h>
#include <couchit/changes.h>
#include <imtjson/refcnt.h>
#include <imtjson/rpc.h>
#include <imtjson/value.h>
#include <thread>

#include "orderControl.h"

using couchit::CouchDB;
namespace quark {

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
		bool stopped;

		FeedControl(CouchDB &db, Value since);
		virtual ~FeedControl() {stop();}

		virtual void init() = 0;
		virtual void onEvent(Value v) = 0;
		void stop();
		void start();


	};

	class BasicFeed;
	class OrderFeed;
	class TradesFeed;
	class PosFeed;

	typedef RefCntPtr<FeedControl> PFeedControl;


	PFeedControl ordersFeed, tradesFeed, posFeed;
	OrderControl orderControl;









};

typedef RefCntPtr<MarketControl> PMarketControl;

} /* namespace quark */

