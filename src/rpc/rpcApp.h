#pragma once
#include <couchit/couchDB.h>
#include <imtjson/value.h>
#include <imtjson/rpc.h>
#include <iosfwd>
#include <mutex>
#include <queue>


#include "marketControl.h"

namespace quark {

using namespace json;
using namespace couchit;

class RpcApp: public RefCntObj {
public:
	RpcApp(Value svctable):svctable(svctable) {}


	void run(std::istream &input, std::ostream &out);



protected:

	void rpcInit(RpcRequest req);

	Value svctable;

	RpcServer rpcServer;
	PMarketControl mcontrol;
	std::mutex streamLock;

	std::queue<Value> cmdQueue;
	std::mutex queueLock;
	std::condition_variable queueTrig;
	std::thread executor;

private:
	Value readQueue();
	void writeQueue(Value v);
};

typedef RefCntPtr<RpcApp> PRpcApp;

}