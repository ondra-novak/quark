/*
 * moneyService.cpp
 *
 *  Created on: 6. 6. 2017
 *      Author: ondra
 */

#include <imtjson/array.h>
#include <imtjson/object.h>
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

void MoneyService::deleteOrder(json::Value user, json::Value order) {
	Sync _(lock);
	budgetMap.erase(Key(user,order));

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
	auto it = lockedUsers.find(user);
	if (it == lockedUsers.end())
		return;
	if (it->second.empty()) {
		lockedUsers.erase(it);
	} else {
		auto req = it->second.front();
		it->second.pop();
		allocBudgetLk2(req);
	}
}


bool MoneyService::allocBudgetLk(const PAllocReq &req) {


	auto lk = lockedUsers.find(req->user);
	if (lk != lockedUsers.end()) {
		lk->second.push(req);
		return false;
	}

	if (client == nullptr) return true;

    lockedUsers[req->user];
	return allocBudgetLk2(req);

}



class MoneyService::AllocCallback {
public:


	AllocCallback(MoneyService &owner, const PAllocReq &req, const OrderBudget &b, bool async)
		:owner(owner),req(req), budget(b), async(async) {}

	void operator()(IMoneySrvClient::AllocResult b) {
		owner.allocFinishCb(async, req, budget, b);
	}

protected:
	MoneyService &owner;
	PAllocReq req;
	OrderBudget budget;
	bool async;
};


void MoneyService::allocFinishCb(bool async, const PAllocReq &req,
		const OrderBudget &budget, IMoneySrvClient::AllocResult b) {
	dispatch << [=] {
		if (b == IMoneySrvClient::allocTryAgain) {
			Sync _(lock);
			client->allocBudget(req->user,budget,AllocCallback(*this, req, budget, async));
		} else {
			inflight.dec();
			if (async) allocFinish(req, b);
			Sync _(lock);
			unlockUser(req->user);
		}
	};
}

bool MoneyService::allocBudgetLk2(const PAllocReq &req) {


	auto badv = calculateBudgetAdv(req->user,req->order,req->budget);
	client->adjustBudget(req->user,badv.first);
	client->adjustBudget(req->user,badv.second);


	if (badv.second.above(badv.first)) {
		inflight.inc();
		client->allocBudget(req->user,badv.second,AllocCallback(*this,req,badv.second,true));
		return false;
	} else if (badv.second != badv.first) {
		//when budget is below previous, we can update budget without waiting
		inflight.inc();
		client->allocBudget(req->user, badv.second,AllocCallback(*this,req,badv.second,false));
		//cannot call allocFinish because it calls callback, but we don't want to call it
		//just update budget
		updateBudget(req->user,req->order,req->budget);
		//however, unlock user must be called from finish
		return true;
	} else {
		//update transaction module, but no need to wait for result
		//and also no need to lock user
		client->allocBudget(req->user, badv.second,[](IMoneySrvClient::AllocResult){});
		//nothing async is needed, update budget now
		updateBudget(req->user,req->order,req->budget);
		//and unlock user
		unlockUser(req->user);
		//its ok
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
		budgetMap[Key(user,order)] =  toBlock;
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
		lockedUsers.clear();
		}
	inflight.zeroWait();
	logDebug("MoneyService destructor");
}

Value quark::MoneyService::toJson() const {
	Sync _(lock);
	Object bmap;
	for (auto &&x: budgetMap) {
		auto u = bmap.object(x.first.user.toString());
		u.set(x.first.command.toString(), x.second.toJson());
	}
	Object lusrs;
	for (auto &&x: lockedUsers) {
		auto rq = lusrs.object(x.first.toString());

		auto q = x.second;
		while (!q.empty()) {
			auto && z = q.front();
			rq.set(z->order.toString(),z->budget.toJson());
			q.pop();
		}
	}
	return Object ("allocated", bmap)
			("waiting", lusrs)
			("processing", inflight.getCounter());
}


/*
OrderBudget MoneyService::getOrderBudget(const json::Value& user, const json::Value& order) const {

	auto f = budgetMap.find(Key(user,order));
	if (f == budgetMap.end()) return OrderBudget();
	else return  f->second;
}

*/
} /* namespace quark */

