#pragma once
#include <couchit/couchDB.h>
#include <couchit/changes.h>
#include <imtjson/refcnt.h>
#include <imtjson/rpc.h>
#include <imtjson/value.h>
#include <thread>

using couchit::CouchDB;
namespace quark {

using namespace json;

class MarketControl: public RefCntObj {
public:
	MarketControl(Value cfg);

	void initRpc(RpcServer &rpcServer);

protected:

	CouchDB ordersDb;
	CouchDB tradesDb;


	static couchit::Config initConfig(Value cfg, StrViewA suffix);

	void rpcOrderCreate(RpcRequest rq);
	void rpcOrderUpdate(RpcRequest rq);
	void rpcOrderCancel(RpcRequest rq);
	void rpcOrderGet(RpcRequest rq);
	void rpcStreamOrders(RpcRequest rq);




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

	typedef RefCntPtr<FeedControl> PFeedControl;


	PFeedControl ordersFeed, tradesFeed;









};

typedef RefCntPtr<MarketControl> PMarketControl;

} /* namespace quark */

