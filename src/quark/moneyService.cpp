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
		//call money service asynchronously
		if (!client->allocBudget(user,badv.second,[=](IMoneySrvClient::AllocResult b){
				if (b == IMoneySrvClient::allocOk) me->updateBudget(user,order,budget);
				if (callback) callback(b);
				}) && callback != nullptr)
			return false;
		else {
			//in case that callback is null, caller doesn't expects a response,
			//we must update local map now, because caller may generate new call sooner
			//in situation when such blocking fails, overblock can happen, and order
			//remains in the map rejecting other orders
		}
	} else if (badv.second != badv.first) {
		//when budget is below previous, we can update budget without waiting
		client->allocBudget(user, badv.second,nullptr);
	}
	//update local map
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
	std::thread thr([=] {callback(IMoneySrvClient::allocReject);});
	thr.detach();
	return false;
}

OrderBudget MoneyService::getOrderBudget(const json::Value& user, const json::Value& order) const {

	std::lock_guard<std::mutex> _(requestLock);
	auto f = budgetMap.find(Key(user,order));
	if (f == budgetMap.end()) return OrderBudget();
	else return  f->second;
}


} /* namespace quark */

