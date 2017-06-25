/*
 * moneyService.cpp
 *
 *  Created on: 6. 6. 2017
 *      Author: ondra
 */

#include "moneyService.h"

#include "logfile.h"

namespace quark {


bool MoneyService::allocBudget(json::Value user,
		json::Value order, const OrderBudget& budget,
		Callback callback) {

	PMoneyService me(this);
	auto badv = calculateBudgetAdv(user,order,budget);
	client->adjustBudget(user,badv.first);
	client->adjustBudget(user,badv.second);
	if (badv.second.above(badv.first)) {
		if (!client->allocBudget(user,badv.second,[=](bool b){
			if (b) me->updateBudget(user,order,budget);
			if (callback) callback(b);
			})) return false;
	} else if (badv.second != badv.first) {
		client->allocBudget(user, badv.second,nullptr);
	} else {
		return true;
	}
	updateBudget(user, order, budget);
	return true;
}


std::pair<OrderBudget,OrderBudget> MoneyService::calculateBudgetAdv(Value user, Value order, const OrderBudget &b) const {
	std::lock_guard<std::mutex> _(requestLock);
	OrderBudget pre;
	OrderBudget post;
	auto low = budgetMap.lower_bound(Key::lBound(user));
	auto high = budgetMap.upper_bound(Key::uBound(user));
	for (auto iter = low; iter!= high; ++iter) {
			pre = pre + iter->second;
			if (iter->first.command != order)
				post = post+iter->second;

	}
	post = post + b;
	pre.adjust(*mcfg);
	post.adjust(*mcfg);
	return std::make_pair(pre,post);
}

void MoneyService::updateBudget(json::Value user, json::Value order, const OrderBudget& toBlock) {
	std::lock_guard<std::mutex> _(requestLock);
	if (toBlock == OrderBudget())
		budgetMap.erase(Key(user,order));
	else
		budgetMap.insert(std::make_pair(Key(user,order), toBlock));
}

bool ErrorMoneyService::allocBudget(json::Value user, OrderBudget total, Callback callback) {
	std::thread thr([=] {callback(false);});
	thr.detach();
	return false;
}


} /* namespace quark */

