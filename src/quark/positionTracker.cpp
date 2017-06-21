/*
 * positionTracker.cpp
 *
 *  Created on: Jun 21, 2017
 *      Author: ondra
 */

#include "positionTracker.h"

#include <couchit/document.h>

namespace quark {



PositionTracker::PositionTracker(CouchDB& posDB):posDB(posDB) {
}

static String getBlockID(Value user) {
	String id = {"block-",user.toString()};
	return id;
}

bool PositionTracker::allocBudget(json::Value user, json::Value order, const OrderBudget& budget, Callback callback) {
	if (budget.context != OrderContext::margin) return false;
	if (budget.type != OrderBudget::asset) return false;
	String id = getBlockID(user);
	do {
		Document doc = posDB.get(id, CouchDB::flgCreateNew);
		double curPos = doc["position"].getNumber();



	} while (true);


	return false;
}

Value PositionTracker::reportTrade(Value prevTrade, Value id, double price, double size, OrderDir::Type dir, std::size_t timestamp) {
}

bool PositionTracker::reportBalanceChange(Value trade, Value user, OrderContext::Type context, double assetChange, double currencyChange, double fee) {
}

} /* namespace quark */
