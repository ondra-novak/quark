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
	rpcServer.add_listMethods("methods");
	rpcServer.add_ping("ping");
	rpcServer.add_multicall("multicall");
	rpcServer.add("init", me, &RpcApp::rpcInit);
	rpcServer.add("delay",[] (RpcRequest req) {
		if (!req.checkArgs(Value(json::array,{"number"}))) return req.setArgError();
		std::this_thread::sleep_for(std::chrono::seconds(req.getArgs()[0].getUInt()));
		req.setResult(true);
	});

	do {
		int c = input.get();
		while (c != EOF && isspace(c)) {
			c = input.get();
		}
		if (c == EOF) return;
		input.putback(c);
		Value v = Value::fromStream(input);
		RpcRequest rq = RpcRequest::create(v, [&](Value response) {
			std::lock_guard<std::mutex> _(streamLock);
			response.toStream(output);
			output << std::endl;
		},RpcFlags::notify);
		rpcServer(rq);



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
