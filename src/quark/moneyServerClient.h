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

	typedef std::function<void()> Action;
	typedef std::function<void(Action)> Dispatch;

	MoneyServerClient(PMoneySvcSupport support, String addr, String signature, String asset, String currency);
	~MoneyServerClient();


	virtual void adjustBudget(json::Value user, OrderBudget &budget) override;
	virtual bool allocBudget(json::Value user, OrderBudget total, Callback callback) override;
	virtual void reportTrade(Value prevTrade, const TradeData &data) override;
	virtual void reportBalanceChange(const BalanceChange &data) override;
	virtual void commitTrade(Value tradeId) override;



protected:

	///dispatcher - it is used to perform connects and post commands from replyes and errors
	PMoneySvcSupport support;

	///connect addr
	const String addr;
	///signature of the client
	const String signature;

	const String asset;

	const String currency;
	///current connection, if nullptr then no conection is active
	PNetworkConection curConn;
	///cancel function for reading  worker
	CancelFunction cancelFn;
	///this is set to true if need to log in to the connection
	/** the flag is need to avoid multiple logins
	 *  first login sets this flag to false causing that any other enqueued login
	 *  is skipped until the login is needed again
	 */
	bool needLogin;

	struct Response {
		Value result;
		Value error;
		std::size_t id;
	};


	typedef std::function<void(Response)> ResponseCb;

	struct ReqInfo {
		String method;
		Value args;
		ResponseCb callback;
		bool canRepeat;

		ReqInfo(String method, Value args, ResponseCb callback, bool canRepeat)
			:method(method),args(args), callback(callback),canRepeat(canRepeat) {}
	};




	typedef std::unordered_map<std::size_t, ReqInfo> PendingRequests;
	typedef std::unique_lock<std::mutex> Sync;
	///array of pending requests
	PendingRequests pendingRequest;
	std::mutex connlock; //locks access a shared state of the connection
	std::mutex maplock; //locks access a shared state of the connection

	std::thread worker;

	Value lastTrade;
	Value lastStoredTrade;
	std::size_t idcounter;
	std::mutex pendingConnect;



protected:

	void stopWorker();
	bool connect(int trynum = 0);
	void disconnect();
	Value registerRequestLk(String method, Value args, ResponseCb callback, bool canRepeat);
	void sendRequestLk(const Value &rq);
	void sendRequest(String method, Value args, ResponseCb callback, bool canRepeat);
	void sendNotify(String method, Value args);
	void handleError(const Response &resp);
	void onNotify(Value resp);
	void workerProc(PNetworkConection conn);
	PNetworkConection getConnection();
	void login();

	class BudgetAllocCallback;
};

} /* namespace quark */


