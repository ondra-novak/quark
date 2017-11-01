#pragma once
#include <couchit/couchDB.h>
#include <imtjson/value.h>
#include <imtjson/rpc.h>
#include <iosfwd>
#include <mutex>
#include <queue>
#include "shared/msgqueue.h"




#include "marketControl.h"

namespace quark {

using namespace json;
using namespace couchit;
using ondra_shared::MsgQueue;



class RpcApp: public RefCntObj {
public:
	RpcApp(Value config):config(config) {}


	void run(std::istream &input, std::ostream &out);



protected:

	void rpcInit(RpcRequest req);
	void rpcEnableBinary(RpcRequest req,std::istream &input, std::ostream &out);
	void streamLog(RpcRequest req);

	Value config;

	RpcServer rpcServer;
	PMarketControl mcontrol;
	std::mutex streamLock;

	std::thread executor;

	MsgQueue<Value> msgQueue;




private:
	void goInteractiveMode(std::istream &input);
	void sendResponse(const Value& response, std::ostream& output);
};

typedef RefCntPtr<RpcApp> PRpcApp;

}
