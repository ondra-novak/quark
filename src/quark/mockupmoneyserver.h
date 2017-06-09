#pragma once
#include "moneyService.h"

namespace quark {


class MockupMoneyService: public AbstractMoneyService {
public:

	MockupMoneyService(BlockedBudget maxBudgetPerUser, std::size_t serverLatency):maxBudgetPerUser(maxBudgetPerUser),serverLatency(serverLatency) {}
	~MockupMoneyService() {stop();}


	void start();
	void stop();

	virtual void requestBudgetOnServer(json::Value user, BlockedBudget total, Callback callback);

protected:
	typedef std::unordered_map<json::Value, BlockedBudget> UserMap;

	std::unique_ptr<std::thread> workerThread;
	UserMap userMap;
	std::size_t serverLatency;
	BlockedBudget maxBudgetPerUser;

	struct QueueItem {
		json::Value user;
		BlockedBudget budget;
		Callback callBack;

		QueueItem(){}
		QueueItem(json::Value user,BlockedBudget budget,Callback callBack)
			:user(user),budget(budget),callBack(callBack) {}
	};

	std::mutex queueLock;
	std::queue<QueueItem> queue;
	std::condition_variable runBackend;
	bool finish = false;

	void worker();

private:
	bool allocBudget(json::Value user, const BlockedBudget &b);
};



}
