#pragma once
#include <couchit/couchDB.h>
#include <imtjson/refcnt.h>
#include <imtjson/rpc.h>
#include <imtjson/value.h>

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

};

typedef RefCntPtr<MarketControl> PMarketControl;

} /* namespace quark */

