#pragma once
#include <couchit/minihttp/netio.h>
#include <imtjson/rpc.h>
#include <thread>

namespace quark {

using namespace json;



class RpcClient: public json::AbstractRpcClient {
public:

	typedef json::AbstractRpcClient Super;

	RpcClient();
	~RpcClient();


	virtual void sendRequest(Value request);

	void enableLogTrafic(bool lt) {logTrafic = true;}

	virtual void onInit() {}
	virtual void onNotify(const Notify &ntf) {}

	String getAddr() const {return addr;}

	///causes disconnecting the client
	/** useful when error found to reconnect later. Note that function is asynchronous, There
	 * still can be request will be processed after the disconect is requested.
	 */
	void disconnect(bool sync = false);
	///Connects stream if necesery. Operation may be asynchronous
	bool  connect(StrViewA addr);

	bool isConnected() const {return connected;}

protected:

	void worker(couchit::PNetworkConection conn);


	String addr;

	couchit::PNetworkConection conn;
	couchit::CancelFunction cancelFn;

	std::thread workerThr;
	bool connected;
	bool logTrafic = false;

private:
	void sendJSON(const Value& v);
	void connectLk(StrViewA addr);
};



}
