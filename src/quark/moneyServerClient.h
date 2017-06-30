#pragma once

#include <mutex>
#include <thread>
#include <unordered_map>
#include <couchit/couchDB.h>
#include <imtjson/string.h>
#include <imtjson/value.h>

#include "imoneysrvclient.h"

namespace quark {


using namespace json;
using namespace couchit;


class MoneyServerClient: public IMoneySrvClient {
public:
	MoneyServerClient(String addr);
	~MoneyServerClient();


	virtual void adjustBudget(json::Value user, OrderBudget &budget);
	virtual bool allocBudget(json::Value user, OrderBudget total, Callback callback);
	virtual Value reportTrade(Value prevTrade, const TradeData &data);
	virtual bool reportBalanceChange(const BalanceChange &data);
	virtual void commitTrade(Value tradeId);



protected:

	String addr;
	PNetworkConection curConn;
	CancelFunction cancelFn;

	struct Response {
		Value result;
		Value error;
		std::size_t id;
	};


	typedef std::function<void(Response)> ResponseCb;

	struct ReqInfo {
		Value request;
		ResponseCb callback;
		bool canRepeat;

		ReqInfo(Value request, ResponseCb callback, bool canRepeat)
			:request(request), callback(callback),canRepeat(canRepeat) {}
	};




	typedef std::unordered_map<std::size_t, ReqInfo> PendingRequests;
	typedef std::unique_lock<std::mutex> Sync;
	PendingRequests pendingRequest;
	std::mutex connlock; //locks access a shared state of the connection
	std::mutex maplock; //locks access a shared state of maps
	std::mutex reqlock; //serialize requests

	std::thread worker;

	Value lastTrade;
	std::size_t idcounter;
	std::size_t initId;

protected:

	void stopWorker();
	void connect();
	PendingRequests::value_type registerRequest(String method, Value args, ResponseCb callback, bool canRepeat);
	void sendRequest(const Value &rq);
	void sendRequest(String method, Value args, ResponseCb callback, bool canRepeat);
	void sendNotify(String method, Value args);
	void handleError(const Response &resp);
	void onNotify(Value resp);

};

} /* namespace quark */


