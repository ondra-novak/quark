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
#include <unordered_set>




#include "orderBudget.h"
#include "marketConfig.h"
#include "imoneysrvclient.h"
#include <common/mtcounter.h>

class Dispatcher;


namespace quark {


///Contains in-memory map of all active orders and their budget
/**
 * Function allocBudget calculates total budget reserved for orders for given user.
 * It calculates budget before update and after update of the budget and determines future
 * action
 *
 * If the budget raising, function asks the money server for permission. Only when
 * permission is granted the order is accepted and added to the in-memory map. Because
 * the request to the moeny server is asynchronous, the function cannot return result
 * immediatelly and expects that caller provides a callback function.
 *
 * If the budget is lowering, function only sends the state to the money server without
 * waiting to reply. In this case, function updates map immediatelly
 */
class MoneyService: public RefCntObj {
public:

	typedef IMoneySrvClient::Callback Callback;

	class AllocReq: public json::RefCntObj {
	public:
		const json::Value user;
		const json::Value order;
		const OrderBudget budget;
		const Callback callback;

		AllocReq(const json::Value &user,
				const json::Value &order, const OrderBudget& budget,
				const Callback &callback)
			:user(user),order(order),budget(budget),callback(callback) {}
	};

	typedef RefCntPtr<AllocReq> PAllocReq;


	MoneyService(PMoneySrvClient client,
				 PMarketConfig mcfg,
				 Dispatcher &dispatch)
		:client(client),mcfg(mcfg),dispatch(dispatch) {}


	~MoneyService();


	///Allocate user's budget
	/**
	 * @param user user identification
	 * @param order order identification
	 * @param budget budget information
	 * @param callback function called when allocation is complete. Argument can
	 * be nullptr in case, that caller doesn't expect negative reply.
	 *
	 * @retval true operation successed without need to access money server. Callback
	 * will not called
	 * @retval false The result is undetermined, the callback function will be called.
	 *
	 * The argument of the response is true=budget allocated, false=allocation rejected
	 */

	bool allocBudget(json::Value user, json::Value order, const OrderBudget &budget, Callback callback);

	bool allocBudget(const PAllocReq &req);

	//void setMarketConfig(PMarketConfig cfg) {mcfg = cfg;client->setMarketConfig(cfg);}*/

//	OrderBudget getOrderBudget(const json::Value &user, const json::Value &order) const;

	void setClient(PMoneySrvClient client) {this->client = client;}
	void setMarketConfig(PMarketConfig cfg) {this->mcfg = mcfg;}

protected:

	PMoneySrvClient client;
	PMarketConfig mcfg;
	Dispatcher &dispatch;
protected:




	bool allocBudgetLk(const PAllocReq &req);


	struct Key {
		json::Value user;
		json::Value command;

		Key () {}
		Key (json::Value user,json::Value command)
			:user(user),command(command) {}

		static Key lBound(json::Value user) {
			return Key(user, nullptr);
		}
		static Key uBound(json::Value user) {
			return Key(user, json::object);
		}

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
	typedef std::unordered_set<json::Value> LockedUsers;
	typedef std::queue<PAllocReq> AllocQueue;


	BudgetUserMap budgetMap;
	LockedUsers lockedUsers;
	AllocQueue allocQueue;
	MTCounter inflight;
	mutable std::mutex lock;


	void updateBudget(json::Value user, json::Value order, const OrderBudget& toBlock);


	OrderBudget calculateBudget(Value user) const;
	std::pair<OrderBudget,OrderBudget> calculateBudgetAdv(Value user, Value order, const OrderBudget &b) const;

	typedef std::unique_lock<std::mutex> Sync;

	void allocFinish(const PAllocReq &req, IMoneySrvClient::AllocResult b);
	void unlockUser(const json::Value user);
};


typedef json::RefCntPtr<MoneyService> PMoneyService;




class ErrorMoneyService: public IMoneySrvClient {
public:
	virtual void adjustBudget(json::Value user, OrderBudget &total) override {}
	virtual bool allocBudget(json::Value user, OrderBudget total, Callback callback) override;
	virtual void reportTrade(Value , const TradeData &d) override {}
	virtual void setMarketConfig(PMarketConfig) {}
};
}

