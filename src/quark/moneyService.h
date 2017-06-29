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




#include "orderBudget.h"
#include "marketConfig.h"
#include "imoneysrvclient.h"

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

	MoneyService(PMoneySrvClient client, PMarketConfig mcfg):client(client),mcfg(mcfg) {}

	typedef std::function<void(bool)> Callback;

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


	//void setMarketConfig(PMarketConfig cfg) {mcfg = cfg;client->setMarketConfig(cfg);}*/

	OrderBudget getOrderBudget(const json::Value &user, const json::Value &order) const;

	void setClient(PMoneySrvClient client) {this->client = client;}
	void setMarketConfig(PMarketConfig cfg) {this->mcfg = mcfg;}

protected:



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

	PMoneySrvClient client;
	BudgetUserMap budgetMap;
	mutable std::mutex requestLock;


	void updateBudget(json::Value user, json::Value order, const OrderBudget& toBlock);

	PMarketConfig mcfg;

	OrderBudget calculateBudget(Value user) const;
	std::pair<OrderBudget,OrderBudget> calculateBudgetAdv(Value user, Value order, const OrderBudget &b) const;
};


typedef json::RefCntPtr<MoneyService> PMoneyService;




class ErrorMoneyService: public IMoneySrvClient {
public:
	virtual void adjustBudget(json::Value user, OrderBudget &total) override {}
	virtual bool allocBudget(json::Value user, OrderBudget total, Callback callback) override;
	virtual Value reportTrade(Value , const TradeData &d) override {return d.id;}
	virtual bool reportBalanceChange(const BalanceChange &)  override {return true;}
	virtual void commitTrade(Value tradeId) {}
	virtual void setMarketConfig(PMarketConfig) {}
};
}

