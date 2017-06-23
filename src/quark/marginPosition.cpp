/*
 * positionTracker.cpp
 *
 *  Created on: Jun 21, 2017
 *      Author: ondra
 */

#include "marginPosition.h"

#include <couchit/changes.h>

#include <couchit/document.h>
#include <couchit/query.h>

namespace quark {



PositionTracker::PositionTracker(CouchDB& posDB, PMoneyService target):posDB(posDB),target(target) {
}

bool PositionTracker::allocBudget(json::Value user, json::Value order, const OrderBudget& budget, Callback callback) {

	if (budget.margin == 0) return target->allocBudget(user,order,budget,callback);

	return false;
}

Value PositionTracker::reportTrade(Value prevTrade, Value id, double price, double size, OrderDir::Type dir, std::size_t timestamp) {
	if (prevTrade != this->prevTrade) return this->prevTrade;
	lastPrice = price;
}


static double sign(double v) {
	return v<0?-1.0:v>0?1.0:0.0;
}

bool PositionTracker::reportBalanceChange(Value trade, Value user, OrderContext::Type context, double assetChange, double currencyChange, double fee) {
	if (context == OrderContext::margin) {
		Document doc = positionMap[user];
		doc.setID(user);
		double prevPos = doc["position"].getNumber();
		double prevValue = doc["value"].getNumber();
		double newPos = prevPos+assetChange;//TODO: adjust position decimal numbers
		double newValue = prevValue+currencyChange;
		if (sign(newPos) != sign(prevPos)) newValue = -newPos * lastPrice;
		doc("position", newPos);
		doc("value", newValue);
		doc("lastTrade",trade);
		doc.enableTimestamp();
		updatePosition(doc);
	}
}

void PositionTracker::syncPositions() {
	positionMap.clear();
	Query q = posDB.createQuery(View::includeDocs);
	q.prefixString("p.");
	Result res = q.exec();
	for (Row rw : res) {
		positionMap[rw.doc["_id"]] = rw.doc;
	}
	ChangesFeed chfd = posDB.createChangesFeed();
	Changes chc = chfd.reversedOrder(true).includeDocs(true).limit(1).setTimeout(0).exec();
	if (chc.hasItems()) {
		prevTrade = ChangedDoc(chc.getNext()).doc["lastTrade"];
	} else {
		prevTrade = nullptr;
	}
}

void PositionTracker::updatePosition(Document doc) {
	posDB.put(doc);
	positionMap[doc.getIDValue()] = doc;

}

} /* namespace quark */
