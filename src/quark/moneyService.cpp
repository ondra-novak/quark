/*
 * moneyService.cpp
 *
 *  Created on: 6. 6. 2017
 *      Author: ondra
 */

#include "moneyService.h"

#include "logfile.h"

namespace quark {

bool MoneyService::sendServerRequest(AllocationResult r, json::Value user,
		OrderBudget total, Callback callback) {
	switch (r) {
	case allocNeedSync:
		return client->allocBudget(user, total, callback);
	case allocNoChange:
		return true;
	case allocAsync:
		client->allocBudget(user, total, nullptr);
		return true;
	}
}

bool MoneyService::allocBudget(json::Value user,
		json::Value order, const OrderBudget& budget,
		Callback callback) {

	OrderBudget total;
	AllocationResult  r  = updateBudget(user,order,budget, total);
	return sendServerRequest(r, user, total, callback);

}


OrderBudget MoneyService::calculateBudget(Value user) const {
	OrderBudget total;
	auto low = budgetMap.lower_bound(Key::lBound(user));
	auto high = budgetMap.upper_bound(Key::uBound(user));
	for (auto iter = low; iter!= high; ++iter) {
			total = total + iter->second;

	}
	return total;

}

MoneyService::AllocationResult MoneyService::updateBudget(json::Value user,
		json::Value order, const OrderBudget& toBlock, OrderBudget& total) {

	std::lock_guard<std::mutex> _(requestLock);
	AllocationResult r;
	Key k(user,order);

	OrderBudget prevBudget = calculateBudget(user);
	client->adjustBudget(user,prevBudget);

	auto p = budgetMap.find(k);

	if (toBlock == OrderBudget()) {
		if (p == budgetMap.end()) {
			total = prevBudget;
			return allocNoChange;
		} else {
			budgetMap.erase(p);
		}
	} else{
		if (p == budgetMap.end()) {
			budgetMap.insert(std::make_pair(k, toBlock));
		} else{
			p->second = toBlock;
		}
	}
	total = calculateBudget(user);
	client->adjustBudget(user,total);
	if (total.above(prevBudget)) {
		return allocNeedSync;
	} else if (total == prevBudget) {
		return allocNoChange;
	} else {
		return allocNeedSync;
	}
}

bool ErrorMoneyService::allocBudget(json::Value user, OrderBudget total, Callback callback) {
	std::thread thr([=] {callback(false);});
	thr.detach();
	return false;
}


} /* namespace quark */

