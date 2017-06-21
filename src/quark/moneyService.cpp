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
		OrderBudget total, Callback callback) {
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
		json::Value order, const OrderBudget& budget,
		Callback callback) {

	OrderBudget total;
	AllocationResult  r  = updateBudget(user,order,budget, total);
	return sendServerRequest(r, user, total, callback);

}


AbstractMoneyService::AllocationResult AbstractMoneyService::updateBudget(json::Value user,
		json::Value order, const OrderBudget& toBlock, OrderBudget& total) {

	std::lock_guard<std::mutex> _(requestLock);
	AllocationResult r;
	Key k(user,order,total.context, total.type);

	auto p = budgetMap.find(k);

	if (toBlock == OrderBudget()) {
		if (p == budgetMap.end()) {
			r = allocNoChange;
		} else {
			r = allocAsync;
			budgetMap.erase(p);
		}
	} else{
		if (p == budgetMap.end()) {
			budgetMap.insert(std::make_pair(k, toBlock.value));
			r = allocNeedSync;
		} else{
			double &b = p->second;;
			if (toBlock.value > b) {
				r = allocNeedSync;
			} else if (toBlock.value == b) {
				r = allocNoChange;
			} else {
				r = allocAsync;
			}
			b = toBlock.value;
		}
	}

	double sum = 0;
	auto low = budgetMap.lower_bound(Key::lBound(user, total.context, total.type));
	auto high = budgetMap.upper_bound(Key::uBound(user, total.context, total.type));
	for (auto iter = low; iter!= high; ++iter) {
			sum = sum + iter->second;

	}
	total = OrderBudget(toBlock.context, toBlock.type,  sum);
	return r;
}

void ErrorMoneyService::requestBudgetOnServer(json::Value user, OrderBudget total, Callback callback) {
	std::thread thr([=] {callback(false);});
	thr.detach();
}


} /* namespace quark */

