/*
 * MarketControl.cpp
 *
 *  Created on: Jun 11, 2017
 *      Author: ondra
 */

#include "marketControl.h"

#include <couchit/couchDB.h>
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

	rpcServer.add("Order.create", notImpl);
	rpcServer.add("Order.modify", notImpl);
	rpcServer.add("Order.cancel", notImpl);
	rpcServer.add("Stream.orders", notImpl);
	rpcServer.add("Stream.trades", notImpl);

}



}
/* namespace quark */


