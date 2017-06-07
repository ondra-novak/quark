#include "mockupmoneyserver.h"

#include "logfile.h"

namespace quark {

void MockupMoneyService::start() {
	if (workerThread != nullptr)
		workerThread = std::unique_ptr<std::thread>(new std::thread([=]{worker();}));
}

void MockupMoneyService::requestBudgetOnServer(json::Value user,
						BlockedBudget total, Callback callback) {
	std::lock_guard<std::mutex> _(queueLock);
	if (workerThread == nullptr) {
		start();
	}
	queue.push(QueueItem(user,total,callback));
	runBackend.notify_all();
}

void MockupMoneyService::stop() {
	if (workerThread != nullptr) {
		finish = true;
		runBackend.notify_all();
		workerThread->join();
	}
}

void MockupMoneyService::worker() {
	while (!finish) {

		std::queue<QueueItem> cpyq;
		{
			std::unique_lock<std::mutex> _(queueLock);
			runBackend.wait(_, [&]{return !queue.empty()||finish;});
			logInfo({"Moneyserver processing batch", queue.size()});
			std::swap(cpyq, queue);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(serverLatency));

		while (!cpyq.empty()) {
			QueueItem itm = cpyq.front();
			cpyq.pop();

			bool res = allocBudget(itm.user,itm.budget);
			if (itm.callBack) itm.callBack(res);
		}
	}
}

bool MockupMoneyService::allocBudget(json::Value user, const BlockedBudget& b) {
	BlockedBudget &cur = userMap[user];
	BlockedBudget final = cur + b;
	if (final.raisedThen(maxBudgetPerUser)) {
		logError({"Rejected budget allocation:",user,b.toJson(),cur.toJson()});
		if (cur == BlockedBudget()) {
			logInfo({"Budget user erased",user});
			userMap.erase(user);
		}
		return false;
	} else {
		cur = final;
		logInfo({"Accepted budget allocation:",user,b.toJson(),final.toJson()});
		if (cur == BlockedBudget()) {
			logInfo({"Budget user erased",user});
			userMap.erase(user);
		}
		return true;
	}
}

}
