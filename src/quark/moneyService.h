#pragma once

#include <imtjson/value.h>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>




#include "blockedBudget.h"

namespace quark {

///Connects to money server and allocates budget for users
/** It executes requests in batches. It also tracks blocked budget for opened commands*/
class AbstractMoneyService: public json::RefCntObj {
public:

	typedef std::function<void(bool)> Callback;

	///Allocate user's budget
	/**
	 * @param user user identification
	 * @param order order identification
	 * @param budget budget information
	 * @param callback function called when allocation is complete.
	 *
	 * @note if function decides to access the money server, the response can be called
	 * asynchronously anytime later. If the request can be processed immediatelly, or
	 * without need to wait for the money server, the response is called also immediately.
	 *
	 * The argument of the response is true=budget allocated, false=allocation rejected
	 */

	bool allocBudget(json::Value user, json::Value order, const OrderBudget &budget, Callback callback);


	virtual void requestBudgetOnServer(json::Value user, OrderBudget total, Callback callback) = 0;

protected:

	struct Key {
		json::Value user;
		json::Value command;

		Key () {}
		Key (json::Value user,json::Value command):user(user),command(command) {}
	};

	struct CmpKey {
		bool operator()(const Key &a, const Key &b) const {
			int r = json::Value::compare(a.user,b.user) ;
			if (r < 0) return true;
			if (r == 0) {
				return  json::Value::compare(a.command,b.command)<0;
			}
			return false;
		}
	};

	typedef std::map<Key, OrderBudget, CmpKey> BudgetUserMap;

	BudgetUserMap budgetMap;
	std::mutex requestLock;

	enum AllocationResult {
		///Budget increased - need synchronous acknowledge
		allocNeedSync,
		///Budget did not changed - no action needed
		allocNoChange,
		///Budget decreased - asynchronous call is enough
		allocAsync,
	};
	bool sendServerRequest(AllocationResult r, json::Value user,
			OrderBudget total, Callback callback);


	AllocationResult updateBudget(json::Value user,json::Value order,
			const OrderBudget &toBlock, OrderBudget &total);
};


typedef json::RefCntPtr<AbstractMoneyService> PMoneyService;


class ErrorMoneyService: public AbstractMoneyService {
public:
	virtual void requestBudgetOnServer(json::Value user, OrderBudget total, Callback callback);
};
}

