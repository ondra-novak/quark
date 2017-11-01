/*
 * moneyService.cpp
 *
 *  Created on: 6. 6. 2017
 *      Author: ondra
 */

#include "shared/dispatcher.h"
#include "moneyService.h"



#include "logfile.h"

namespace quark {


bool MoneyService::allocBudget(json::Value user,
		json::Value order, const OrderBudget& budget,
		Callback callback) {

	Sync _(lock);
	return allocBudgetLk(new AllocReq(user,order,budget,callback));
}

bool MoneyService::allocBudget(const PAllocReq &req) {

	Sync _(lock);
	return allocBudgetLk(req);
}


void MoneyService::allocFinish(const PAllocReq &req, IMoneySrvClient::AllocResult b) {

	switch (b) {
	case IMoneySrvClient::allocOk:
		updateBudget(req->user,req->order,req->budget);
		if (req->callback != nullptr) req->callback(b);
		break;
	case IMoneySrvClient::allocReject:
		if (req->callback != nullptr) req->callback(b);
		break;
	case IMoneySrvClient::allocError:
		if (req->callback != nullptr) req->callback(b);
		break;
	case IMoneySrvClient::allocTryAgain:
		break;
	};
}

void MoneyService::unlockUser(const json::Value user) {
	lockedUsers.erase(user);
	if (!allocQueue.empty()) {
		auto q = allocQueue.front();
		allocQueue.pop();
		allocBudgetLk(q);
	}
	if (!allocQueue.empty()) {
		auto q = allocQueue.front();
		allocQueue.pop();
		allocBudgetLk(q);
	}
}


bool MoneyService::allocBudgetLk(const PAllocReq &req) {
	if (lockedUsers.find(req->user) != lockedUsers.end()) {
		allocQueue.push(req);
		return false;
	}

	if (client == nullptr) return true;

	lockedUsers.insert(req->user);

	auto badv = calculateBudgetAdv(req->user,req->order,req->budget);
	client->adjustBudget(req->user,badv.first);
	client->adjustBudget(req->user,badv.second);


	auto finishAsync = [=](IMoneySrvClient::AllocResult b) {
		dispatch << [=] {
			if (b == IMoneySrvClient::allocTryAgain) {
				Sync _(lock);
				unlockUser(req->user);
				allocBudgetLk(req);
			} else {
				allocFinish(req, b);
				Sync _(lock);
				unlockUser(req->user);
			}
			inflight.dec();
		};
	};

	auto finishSync = [=](IMoneySrvClient::AllocResult) {
		dispatch << [=] {
			Sync _(lock);
			unlockUser(req->user);
			inflight.dec();
		};
	};

	if (badv.second.above(badv.first)) {
		inflight.inc();
		client->allocBudget(req->user,badv.second,finishAsync);
		return false;
	} else if (badv.second != badv.first) {
		//when budget is below previous, we can update budget without waiting
		inflight.inc();
		client->allocBudget(req->user, badv.second,finishSync);
		finishAsync(IMoneySrvClient::allocOk);
		return true;
	} else {
		finishAsync(IMoneySrvClient::allocOk);
		return true;
	}


}


std::pair<OrderBudget,OrderBudget> MoneyService::calculateBudgetAdv(Value user, Value order, const OrderBudget &b) const {
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
	Sync _(lock);
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


quark::MoneyService::~MoneyService() {
		{
		Sync _(lock);
		client = nullptr;
		allocQueue = AllocQueue();
		}
	inflight.zeroWait();
	logDebug("MoneyService destructor");
}

/*
OrderBudget MoneyService::getOrderBudget(const json::Value& user, const json::Value& order) const {

	auto f = budgetMap.find(Key(user,order));
	if (f == budgetMap.end()) return OrderBudget();
	else return  f->second;
}

*/
} /* namespace quark */

