#pragma once
#include <couchit/minihttp/netio.h>
#include <imtjson/rpc.h>
#include <thread>

namespace quark {

using namespace json;



class RpcClient: public json::AbstractRpcClient {
public:

	typedef json::AbstractRpcClient Super;

	RpcClient(String addr);
	~RpcClient();


	virtual void sendRequest(Value request);

	void stopWorker();

	virtual void onInit() {}
	virtual void onNotify(const Notify &ntf) {}

protected:

	void worker(couchit::PNetworkConection conn);

	String addr;

	couchit::PNetworkConection conn;
	couchit::CancelFunction cancelFn;

	std::thread workerThr;
	bool running;

private:
	void sendJSON(const Value& v);
};


}
