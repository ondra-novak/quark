/*
 * positionTracker.cpp
 *
 *  Created on: Jun 21, 2017
 *      Author: ondra
 */

#include "marginPosition.h"

#include <couchit/document.h>

namespace quark {



PositionTracker::PositionTracker(CouchDB& posDB, PMoneyService target):posDB(posDB),target(target) {
}

bool PositionTracker::allocBudget(json::Value user, json::Value order, const OrderBudget& budget, Callback callback) {

	if (budget.margin == 0) return target->allocBudget(user,order,budget,callback);

	return false;
}

Value PositionTracker::reportTrade(Value prevTrade, Value id, double price, double size, OrderDir::Type dir, std::size_t timestamp) {
}

bool PositionTracker::reportBalanceChange(Value trade, Value user, OrderContext::Type context, double assetChange, double currencyChange, double fee) {
}

} /* namespace quark */
