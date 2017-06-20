#pragma once
#include "moneyService.h"

namespace quark {


class MockupMoneyService: public AbstractMoneyService {
public:

	MockupMoneyService(double maxAssets, double maxCurrency, std::size_t serverLatency):maxAssets(maxAssets),maxCurrency(maxCurrency),serverLatency(serverLatency) {}
	~MockupMoneyService() {stop();}


	void start();
	void stop();

	virtual void requestBudgetOnServer(json::Value user, OrderBudget total, Callback callback);

protected:

	std::unique_ptr<std::thread> workerThread;
	std::size_t serverLatency;
	double maxAssets;
	double maxCurrency;

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
