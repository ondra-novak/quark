/*
 * rpcApp.cpp
 *
 *  Created on: Jun 11, 2017
 *      Author: ondra
 */

#include "rpcApp.h"

#include <imtjson/binjson.tcc>
#include <imtjson/utf8.h>

namespace quark {


void RpcApp::run(std::istream& input, std::ostream& output) {


	rpcServer.add_listMethods();
	rpcServer.add_ping();
	rpcServer.add_multicall();

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

} /* namespace quark */
