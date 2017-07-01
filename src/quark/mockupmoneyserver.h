#pragma once

#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include "orderBudget.h"
#include "imoneysrvclient.h"


namespace quark {


class MockupMoneyService: public IMoneySrvClient {
public:

	MockupMoneyService(OrderBudget maxBudget, std::size_t serverLatency):maxBudget(maxBudget),serverLatency(serverLatency) {}
	~MockupMoneyService() {stop();}


	void start();
	void stop();

	virtual void adjustBudget(json::Value user, OrderBudget &budget) override ;
	virtual bool allocBudget(json::Value user, OrderBudget total, Callback callback) override ;
	virtual void reportTrade(Value prevTrade, const TradeData &data) override ;
	virtual void reportBalanceChange(const BalanceChange &data) override ;
	virtual void commitTrade(Value tradeId) override ;

protected:

	std::unique_ptr<std::thread> workerThread;
	std::size_t serverLatency;
	OrderBudget maxBudget;

	struct QueueItem {
		json::Value user;
		OrderBudget budget;
		Callback callBack;

		QueueItem(){}
		QueueItem(json::Value user,OrderBudget budget,Callback callBack)
			:user(user),budget(budget),callBack(callBack) {}
	};

	std::mutex queueLock;
	std::queue<QueueItem> queue;
	std::condition_variable runBackend;
	bool finish = false;

	void worker();

private:
	bool allocBudget(json::Value user, const OrderBudget &b);
};



}
