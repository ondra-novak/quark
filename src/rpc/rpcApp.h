#pragma once
#include <couchit/couchDB.h>
#include <imtjson/value.h>
#include <imtjson/rpc.h>
#include <iosfwd>
#include <mutex>
#include <queue>
#include "../common/msgqueue.h"


#include "marketControl.h"

namespace quark {

using namespace json;
using namespace couchit;

class RpcApp: public RefCntObj {
public:
	RpcApp(Value config):config(config) {}


	void run(std::istream &input, std::ostream &out);



protected:

	void rpcInit(RpcRequest req);

	Value config;

	RpcServer rpcServer;
	PMarketControl mcontrol;
	std::mutex streamLock;

	std::thread executor;

	MsgQueue<Value> msgQueue;

private:
	void goInteractiveMode(std::istream &input);
};

typedef RefCntPtr<RpcApp> PRpcApp;

}
