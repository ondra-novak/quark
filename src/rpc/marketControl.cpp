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
	rpcServer.add("Order.modify", notImpl);
	rpcServer.add("Order.cancel", notImpl);
	rpcServer.add("Stream.orders", notImpl);
	rpcServer.add("Stream.trades", notImpl);

}


void MarketControl::rpcOrderCreate(RpcRequest rq) {
	Value args = rq.getArgs();
	if (args.size() != 1 || args[0].type() != json::object) {
		rq.setArgError(json::undefined);
	}
	CouchDB::PConnection conn = ordersDb.getConnection("_design/orders/_update/order");
	try {
		Value res = ordersDb.requestPOST(conn,args[0],nullptr,0);
		rq.setResult(res["orderId"]);
	} catch (const couchit::RequestError &e) {
		rq.setError(e.getCode(), e.getMessage(),e.getExtraInfo());
	}


}

}

/* namespace quark */


