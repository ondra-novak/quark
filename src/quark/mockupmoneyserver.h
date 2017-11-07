#pragma once

#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>

#include "shared/msgqueue.h"
#include "orderBudget.h"
#include "imoneysrvclient.h"


namespace quark {
using ondra_shared::MsgQueue;


class MockupMoneyService: public IMoneySrvClient {
public:

	MockupMoneyService(OrderBudget maxBudget, std::size_t serverLatency):maxBudget(maxBudget),serverLatency(serverLatency) {
		start();
	}
	~MockupMoneyService() {stop();}


	void start();
	void stop();

	virtual void adjustBudget(json::Value user, OrderBudget &budget) override ;
	virtual bool allocBudget(json::Value user, OrderBudget total, Callback callback) override ;
	virtual void reportTrade(Value prevTrade, const TradeData &data) override ;
	virtual void resync() override;

protected:

	std::thread workerThread;
	std::size_t serverLatency;
	OrderBudget maxBudget;

	struct QueueItem: public RefCntObj {
		json::Value user;
		OrderBudget budget;
		Callback callBack;
		std::chrono::time_point<std::chrono::steady_clock> execTime;

		QueueItem(){}
		QueueItem(json::Value user,OrderBudget budget,Callback callBack,
				std::chrono::time_point<std::chrono::steady_clock> execTime)
			:user(user),budget(budget),callBack(callBack),execTime(execTime) {}
	};

	typedef json::RefCntPtr<QueueItem> PQueueItem;
	MsgQueue<PQueueItem> queue;

	void worker();

private:
	bool allocBudget(json::Value user, const OrderBudget &b);
};



}
