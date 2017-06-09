/*
 * moneyService.cpp
 *
 *  Created on: 6. 6. 2017
 *      Author: ondra
 */

#include "moneyService.h"

#include "logfile.h"

namespace quark {

bool AbstractMoneyService::sendServerRequest(AllocationResult r, json::Value user,
		BlockedBudget total, Callback callback) {
	switch (r) {
	case allocNeedSync:
		requestBudgetOnServer(user, total, callback);
		return false;
	case allocNoChange:
		return true;
	case allocAsync:
		requestBudgetOnServer(user, total, nullptr);
		return true;
	}
}

bool AbstractMoneyService::allocBudget(json::Value user,
		json::Value order, const BlockedBudget& budget,
		Callback callback) {

	BlockedBudget total;
	AllocationResult  r  = updateBudget(user,order,budget, total);
	return sendServerRequest(r, user, total, callback);

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

void ErrorMoneyService::requestBudgetOnServer(json::Value user, BlockedBudget total, Callback callback) {
	std::thread thr([=] {callback(false);});
	thr.detach();
}


} /* namespace quark */

