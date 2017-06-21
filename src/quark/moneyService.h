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
#include "marketConfig.h"

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

	///Report trade executed
	/**
	 * @param prevTrade ID of previous trade. If it doesn't match, function reports nothing and sends known last trade
	 * @param id id of current trade
	 * @param price price of the trade
	 * @param size size of trade (amount)
	 * @param dir taker's action (buy or sell)
	 * @param timestamp unix timestamp of the trade (may duplicate)
	 * @return if trade is reported, function returns id. If not, function retuns last known trade. Function
	 * can return null to start reporting from the beginning
	 */
	virtual Value reportTrade(Value prevTrade, Value id, double price, double size, OrderDir::Type dir, std::size_t timestamp) = 0;
	///Reports balance change for the user
	/**
	 * @param trade ID of trade as reference
	 * @param user ID of user
	 * @param context context (exchange or margin)
	 * @param assetChange (asset change - position change for margin)
	 * @param currencyChange (change of currency - or value of the position for margin trading)
	 * @param fee fee
	 * @retval true reported
	 * @retval false referenced trade was not last trade, please start over
	 */
	virtual bool reportBalanceChange(Value trade, Value user, OrderContext::Type context, double assetChange, double currencyChange, double fee) = 0;


	///Request budget allocation
	/**
	 * @param user user ID
	 * @param total absolute budget for given context
	 * @param callback callback function
	 */
	virtual void requestBudgetOnServer(json::Value user, OrderBudget total, Callback callback) = 0;

	void setMarketConfig(PMarketConfig cfg) {mcfg = cfg;}

protected:

	struct Key {
		json::Value user;
		json::Value command;
		OrderContext::Type context;
		OrderBudget::Type type;

		Key () {}
		Key (json::Value user,json::Value command, OrderContext::Type context, OrderBudget::Type type)
			:user(user),command(command),context(context),type(type) {}

		static Key lBound(json::Value user,OrderContext::Type context, OrderBudget::Type type) {
			return Key(user, nullptr, context,type);
		}
		static Key uBound(json::Value user,OrderContext::Type context, OrderBudget::Type type) {
			return Key(user, json::object, context,type);
		}

	};

	struct CmpKey {
		bool operator()(const Key &a, const Key &b) const {
			int r = json::Value::compare(a.user,b.user) ;
			if (r < 0) return true;
			if (r == 0) {
				if (a.context != b.context) return a.context < b.context;
				if (a.type != b.type) return a.type < b.type;
				return  json::Value::compare(a.command,b.command)<0;
			}
			return false;
		}
	};

	typedef std::map<Key, double, CmpKey> BudgetUserMap;

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

	PMarketConfig mcfg;
};


typedef json::RefCntPtr<AbstractMoneyService> PMoneyService;


class ErrorMoneyService: public AbstractMoneyService {
public:
	virtual void requestBudgetOnServer(json::Value user, OrderBudget total, Callback callback);
	virtual Value reportTrade(Value , Value id, double , double , OrderDir::Type , std::size_t ) {return id;}
	virtual bool reportBalanceChange(Value, Value , OrderContext::Type , double , double , double ) {return true;}

};
}

