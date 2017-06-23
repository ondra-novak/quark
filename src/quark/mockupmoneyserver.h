#pragma once
#include "moneyService.h"

namespace quark {


class MockupMoneyService: public AbstractMoneyService {
public:

	MockupMoneyService(OrderBudget maxBudget, std::size_t serverLatency):maxBudget(maxBudget),serverLatency(serverLatency) {}
	~MockupMoneyService() {stop();}


	void start();
	void stop();

	virtual void requestBudgetOnServer(json::Value user, OrderBudget total, Callback callback);
	virtual Value reportTrade(Value prevTrade, const TradeData &data);
	virtual bool reportBalanceChange(const BalanceChange &data);
	virtual void commitTrade(Value tradeId);


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
