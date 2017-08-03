/*
 * positionTracker.cpp
 *
 *  Created on: Jun 21, 2017
 *      Author: ondra
 */

#include "marginTradingSvc.h"

#include <couchit/changes.h>

#include <couchit/document.h>
#include <couchit/query.h>

namespace quark {



MarginTradingSvc::MarginTradingSvc(CouchDB& posDB, PMarketConfig mcfg, PMoneySrvClient target)
	:posDB(posDB)
	,target(target)
	,wrtx(posDB.createChangeset())
	,mcfg(mcfg)
{
	syncPositions();
}


static inline String user2docid(Value user) {
	return String ({"p.",user.toString()});
}
void MarginTradingSvc::adjustBudget(json::Value user, OrderBudget &budget) {
	if (budget.marginLong != 0 || budget.marginShort != 0) {
		Guard _(lock);
		auto iter = positionMap.find(user2docid(user));
		if (iter != positionMap.end()) {
			const Document &positionDoc = iter->second;
			double curPos = positionDoc["position"].getNumber();
			double curVal = positionDoc["value"].getNumber();
			if (curPos > 0) {
				//long
				if (budget.posShort <= curPos) {
					budget.posShort = 0;
					budget.marginShort = 0;
				} else {
					budget.posShort -= curPos;
					budget.marginShort = std::max(budget.marginShort + curVal,0.0);
				}
			} else if (curPos < 0) {
				if (budget.posLong <= -curPos) {
					budget.posLong = 0;
					budget.marginLong = 0;
				} else {
					budget.posLong += curPos;
					budget.marginLong = std::max(budget.marginLong - curVal,0.0);
				}
			}
		}
	}
	target->adjustBudget(user,budget);
}

bool MarginTradingSvc::allocBudget(json::Value user, OrderBudget budget, Callback callback) {
	return target->allocBudget(user,budget,callback);
}

void MarginTradingSvc::reportTrade(Value prevTrade, const TradeData &data) {
	target->reportTrade(prevTrade, data);

	lastPrice = data.price;
}


static double sign(double v) {
	return v<0?-1.0:v>0?1.0:0.0;
}


/*void MarginTradingSvc::reportBalanceChange(const BalanceChange &data) {
	target->reportBalanceChange(data);
	if (data.context == OrderContext::margin) {
		String docUserId  = user2docid(data.user);
		Document doc = positionMap[Value(docUserId)];
		doc.setID(Value(docUserId));
		double prevPos = doc["position"].getNumber();
		double prevValue = doc["value"].getNumber();
		double newPos = mcfg->adjustSize(prevPos+data.assetChange);//TODO: adjust position decimal numbers
		double newValue = mcfg->adjustPrice(prevValue+data.currencyChange);
		if (sign(newPos) != sign(prevPos)) newValue = mcfg->adjustPrice(-newPos * lastPrice);
		doc("position", newPos);
		doc("value", newValue);
		doc("lastTrade",data.trade);
		doc.enableTimestamp();
		wrtx.update(doc);
	}
}*/

void MarginTradingSvc::syncPositions() {
	positionMap.clear();
	Query q = posDB.createQuery(View::includeDocs);
	q.prefixString("p.");
	Result res = q.exec();
	for (Row rw : res) {
		positionMap[rw.doc["_id"]] = rw.doc;
	}
}

/*void MarginTradingSvc::commitTrade(Value tradeId) {
	wrtx.commit();
	auto &commited = wrtx.getCommitedDocs();
	for (auto &&c : commited) {
		Value newDoc = c.doc.replace("_rev", c.newRev);
		positionMap[newDoc["_id"]] = newDoc;
	}

	target->commitTrade(tradeId);
}*/


void MarginTradingSvc::updatePosition(Document doc) {
	posDB.put(doc);
	Guard _(lock);
	positionMap[doc.getIDValue()] = doc;

}

} /* namespace quark */
