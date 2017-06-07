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


bool MoneyServiceAdapter::allocBudget(json::Value user,
		json::Value orderId, const BlockedBudget& budget,
		AbstractMoneyService::Callback callback) {

	if (isPending(orderId)) {
		BlockedBudget b = budget;
		if (asyncWait(orderId, [=] {
				if (allocBudget(user,orderId,b, callback)) {
					callback(true);
				}
			})) {
				return true;
		}
	}
	std::lock_guard<std::mutex> _(lock);
	bool r = moneyService->allocBudget(user,orderId,budget,[=](bool r) {
		releasePending(orderId);
		callback(r);
	});
	if (!r) {
		pendingOrders.insert(std::make_pair(orderId,nullptr));
	}


}

bool quark::MoneyServiceAdapter::isPending(json::Value orderId) const {
	std::lock_guard<std::mutex> _(lock);
	return pendingOrders.find(orderId) != pendingOrders.end();
}

bool quark::MoneyServiceAdapter::asyncWait(json::Value orderId,
		ReleaseCallback callback) {

	std::lock_guard<std::mutex> _(lock);
	auto p = pendingOrders.find(orderId);
	if (p == pendingOrders.end()) return false;
	auto curFn = p->second;
	p->second = [curFn,callback]{
		if (curFn != nullptr) curFn();
		callback();
	};
}

void MoneyServiceAdapter::setMoneyService(
		std::unique_ptr<AbstractMoneyService>&& moneyService) {
	this->moneyService = std::move(moneyService);
}

void MoneyServiceAdapter::releasePending(json::Value orderId) {
	ReleaseCallback rb;

	while (true) {
		{
			std::lock_guard<std::mutex> _(lock);
			auto p = pendingOrders.find(orderId);
			if (p == pendingOrders.end()) return;
			std::swap(rb,p->second);
			if (rb == nullptr) {
				pendingOrders.erase(p);
				return;
			}
		}
		rb();
	}
}

} /* namespace quark */
