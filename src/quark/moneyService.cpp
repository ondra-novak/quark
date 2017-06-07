/*
 * moneyService.cpp
 *
 *  Created on: 6. 6. 2017
 *      Author: ondra
 */

#include "moneyService.h"

#include "logfile.h"

namespace quark {

void AbstractMoneyService::sendServerRequest(AllocationResult r, json::Value user,
		BlockedBudget total, Callback callback) {
	switch (r) {
	case allocNeedSync:
		requestBudgetOnServer(user, total, callback);
		break;
	case allocNoChange:
		if (callback != nullptr) callback(true);
		break;
	case allocAsync:
		if (callback != nullptr) callback(true);
		requestBudgetOnServer(user, total, nullptr);
		break;
	}
}

void AbstractMoneyService::allocBudget(json::Value user,
		json::Value order, const BlockedBudget& budget,
		Callback callback) {

	BlockedBudget total;
	AllocationResult  r  = updateBudget(user,order,budget, total);
	sendServerRequest(r, user, total, callback);

}


AbstractMoneyService::AllocationResult AbstractMoneyService::updateBudget(json::Value user,
		json::Value order, const BlockedBudget& toBlock, BlockedBudget& total) {

	std::lock_guard<std::mutex> _(requestLock);
	AllocationResult r;
	Key k(user,order);

	auto p = budgetMap.find(k);

	if (toBlock == BlockedBudget()) {
		if (p == budgetMap.end()) {
			r = allocNoChange;
		} else {
			r = allocAsync;
			budgetMap.erase(p);
		}
	} else{
		if (p == budgetMap.end()) {
			budgetMap.insert(std::make_pair(k, toBlock));
			r = allocNeedSync;
		} else{
			BlockedBudget &b = p->second;;
			if (toBlock.raisedThen(b)) {
				r = allocNeedSync;
			} else if (toBlock == b) {
				r = allocNoChange;
			} else {
				r = allocAsync;
			}
			b = toBlock;
		}
	}

	total = BlockedBudget();
	auto low = budgetMap.lower_bound(Key(user,json::null));
	auto high = budgetMap.upper_bound(Key(user,json::object));
	for (auto iter = low; iter!= high; ++iter) {
			total = total +iter->second;
	}
	return r;
}

} /* namespace quark */

void quark::MockupMoneyService::start() {
	if (workerThread != nullptr)
		workerThread = std::unique_ptr<std::thread>(new std::thread([=]{worker();}));
}

void quark::MockupMoneyService::requestBudgetOnServer(json::Value user,
						BlockedBudget total, Callback callback) {
	std::lock_guard<std::mutex> _(queueLock);
	queue.push(QueueItem(user,total,callback));
	runBackend.notify_all();
}

void quark::MockupMoneyService::stop() {
	if (workerThread != nullptr) {
		finish = true;
		runBackend.notify_all();
		workerThread->join();
	}
}

void quark::MockupMoneyService::worker() {
	while (!finish) {
		std::this_thread::sleep_for(std::chrono::milliseconds(serverLatency));
		std::unique_lock<std::mutex> _(queueLock);
		runBackend.wait(_, [&]{return !queue.empty()||finish;});
		logInfo({"Moneyserver processing batch", queue.size()});
		while (!queue.empty()) {
			QueueItem itm = queue.front();
			queue.pop();

			bool res = allocBudget(itm.user,itm.budget);
			if (itm.callBack) itm.callBack(res);
		}
	}
}

bool quark::MockupMoneyService::allocBudget(json::Value user, const BlockedBudget& b) {
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
