/*
 * rpcApp.cpp
 *
 *  Created on: Jun 11, 2017
 *      Author: ondra
 */
#include <unistd.h>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

#include "rpcApp.h"

#include <imtjson/binjson.tcc>
#include <imtjson/utf8.h>
#include "marketControl.h"
#include <stdio.h>


namespace quark {


void RpcApp::streamLog(RpcRequest req) {
    static std::thread thr;


    String cmdline ( config["logReadCmd"] );
    FILE *f = popen(cmdline.c_str(),"r");

    if (f == NULL) {
	req.setError(500,"Open failed", cmdline);
	return;
    }

    thr = std::thread([=]() mutable {
	
	std::vector<char> buffer;
	int i = fgetc(f);
	while (i != EOF) {
	    if (i == '\n') {
		json::StrViewA line (buffer.data(), buffer.size());
		req.sendNotify("log", line);
		buffer.clear();
	    } else {
		buffer.push_back((char)i);
	    }
	    i = fgetc(f);
	}

    });

    req.setResult(true);

}

void RpcApp::sendResponse(const Value& response, std::ostream& output) {
	std::lock_guard<std::mutex> _(streamLock);

		response.toStream(output);
		output << std::endl;
}

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
	rpcServer.add("Stream.log",me,&RpcApp::streamLog);


	executor = std::thread([&]{

		Value v = msgQueue.pop();
		while (v.defined()) {
			RpcRequest rq = RpcRequest::create(v, [&](Value response) {
				sendResponse(response, output);
			},RpcFlags::notify);
			rpcServer(rq);
			v = msgQueue.pop();
		}

	});

#ifdef _WIN32
	 _setmode( _fileno( stdout ),  _O_BINARY );
	 _setmode( _fileno( stdn ),  _O_BINARY );
#endif

	try {
		do {
			int c = input.get();
			Value v;
			{
				while (c != EOF && isspace(c)) {
					c = input.get();
				}
				if (c == 'i') {
					goInteractiveMode(input);
					break;
				}
				if (c == 'q') {
					break;
				}
				if (c == EOF) break;
				input.putback(c);
				 v = Value::fromStream(input);
			}
			msgQueue.push(v);


		} while (true);
		msgQueue.push(json::undefined);
		executor.join();
		mcontrol = nullptr;
	} catch (std::exception &e) {
		msgQueue.push(json::undefined);
		executor.join();
		mcontrol = nullptr;

		Value resp = Object("error",Object("code",-32700)("message","Parse error")("data",e.what()))
						   ("result",nullptr)
						   ("id",nullptr);
		sendResponse(resp,output);

	}
	rpcServer.removeAll();

}

void RpcApp::rpcInit(RpcRequest req) {
	static Value args = Value(json::array,{Object("market","string")});
	if (!req.checkArgs(args)) return req.setArgError();
	StrViewA market = req.getArgs()[0]["market"].getString();
	mcontrol = new MarketControl(config,market);
	if (!mcontrol->testDatabase()) {
		req.setError(404,"Market is not hosted at this node");
	} else {
		req.setResult(mcontrol->initRpc(rpcServer));
	}

}


void RpcApp::goInteractiveMode(std::istream& input) {
	std::cerr << "# Interactive mode is enabled" << std::endl;
	std::cerr << "# Write command in shorter form: <cmd> [args]" << std::endl;
	std::cerr << "# Example: ping[1,2,3]" << std::endl;
	std::cerr << "# methods[] - for list methods" << std::endl;

	std::string method;
	unsigned int cnt=1;
	for(;;) {
		method.clear();
		int c = input.get();
		while (c != EOF && isspace(c)) {
			c = input.get();
		}
		while (c != EOF && !isspace(c) && c != '{' && c != '[') {
			method.push_back(c);
			c = input.get();
		}
		if (c == EOF) {
			return;
		}
		input.putback(c);

		Value args;
		try {
			args = Value::fromStream(input);
		} catch (std::exception &e) {
			std::cerr << e.what();
			continue;
		}
		Value req = Object("method",method)
				("params",args)
				("id", cnt)
				("jsonrpc","2.0");
		cnt++;
		msgQueue.push(req);
	}
}


} /* namespace quark */

