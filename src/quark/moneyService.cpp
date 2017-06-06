/*
 * moneyService.cpp
 *
 *  Created on: 6. 6. 2017
 *      Author: ondra
 */

#include "moneyService.h"

namespace quark {

void AbstractMoneyService::sendServerRequest(AllocationResult r, json::Value user,
		BlockedBudget total, IAllocResponse *response) {
	switch (r) {
	case allocNeedSync:
		requestBudgetOnServer(user, total, response);
		break;
	case allocNoChange:
		response(true);
		break;
	case allocAsync:
		response(true);
		requestBudgetOnServer(user, total, nullptr);
		break;
	}
}

void AbstractMoneyService::allocBudget(json::Value user,
		json::Value order, const BlockedBudget& budget,
		IAllocResponse *response) {

	BlockedBudget total;
	AllocationResult  r  = updateBudget(user,order,budget, total);
	sendServerRequest(r, user, total, response);

}

void AbstractMoneyService::clearBudget(json::Value user,
		json::Value order) {

	BlockedBudget total;
	AllocationResult  r = clearBudget(user,order,total);

}


} /* namespace quark */
