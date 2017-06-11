/*
 * rpcApp.cpp
 *
 *  Created on: Jun 11, 2017
 *      Author: ondra
 */

#include "rpcApp.h"

#include <imtjson/binjson.tcc>
#include <imtjson/utf8.h>
#include "marketControl.h"

namespace quark {


void RpcApp::run(std::istream& input, std::ostream& output) {


	PRpcApp me(this);
	rpcServer.add_listMethods();
	rpcServer.add_ping();
	rpcServer.add_multicall();
	rpcServer.add("init", me, &RpcApp::rpcInit);

	do {
		int c = input.get();
		while (c != EOF && isspace(c)) {
			c = input.get();
		}
		if (c == EOF) return;
		if (c == 0) {
			Value v = Value::parseBinary(fromStream(input));
			RpcRequest rq = RpcRequest::create(v, [&](Value response) {
				response.serializeBinary(toStream(output));
			});
			rpcServer(rq);

		} else {
			input.putback(c);
			Value v = Value::fromStream(input);
			RpcRequest rq = RpcRequest::create(v, [&](Value response) {
				response.toStream(output);
			});
			rpcServer(rq);
		}



	} while (true);


}

void RpcApp::rpcInit(RpcRequest req) {
	static Value args = Value(json::array,{Object("market","string")});
	if (!req.checkArgs(args)) return req.setArgError();
	StrViewA market = req.getArgs()[0]["market"].getString();
	Value cfg = svctable[market];
	if (cfg.defined()) {
		mcontrol = new MarketControl(cfg);
		mcontrol->initRpc(rpcServer);
		req.setResult(true);
	} else {
		req.setError(404,"Market is not hosted at this node");
	}
}

} /* namespace quark */
