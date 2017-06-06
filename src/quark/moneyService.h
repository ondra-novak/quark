#pragma once

namespace quark {

///Connects to money server and allocates budget for users
/** It executes requests in batches. It also tracks blocked budget for opened commands*/
class AbstractMoneyService {
public:

	class IAllocResponse {
	public:
		virtual void responseFn(bool response) = 0;
	};

	///Allocate user's budget
	/**
	 * @param user user identification
	 * @param order order identification
	 * @param budget budget information
	 * @param response function called when allocation is complete.
	 *
	 * @note if function decides to access the money server, the response can be called
	 * asynchronously anytime later. If the request can be processed immediatelly, or
	 * without need to wait for the money server, the response is called also immediately.
	 *
	 * The argument of the response is true=budget allocated, false=allocation rejected
	 */
	template<typename Fn>
	void allocBudget(json::Value user, json::Value order, const BlockedBudget &budget, Fn response);


	///Clears user's budget for the given command
	void clearBudget(json::Value user, json::Value order);


protected:

	void allocBudget(json::Value user, json::Value order, const BlockedBudget &budget,
			IAllocResponse *response);


	struct Key {
		json::Value user;
		json::Value command;
	};

	struct CmpKey {
		bool operator()(const Key &a, const Key &b) const {
			int r = json::Value::compare(a.user,b.user) ;
			if (r < 0) return true;
			if (r == 0) {
				return json::Value::compare(a.command,b.command)<0;
			} else {
				return false;
			}
		}
	};

	typedef std::map<Key, BlockedBudget> BudgetUserMap;

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
	void sendServerRequest(AllocationResult r, json::Value user,
			BlockedBudget total, IAllocResponse *response);


};

}


