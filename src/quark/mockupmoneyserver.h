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
	virtual Value reportTrade(Value prevTrade, Value id, double price, double size, OrderDir::Type dir, std::size_t timestamp);
	virtual bool reportBalanceChange(Value trade, Value user, OrderContext::Type context, double assetChange, double currencyChange, double fee);


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
