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


static double sign(double v) {
	return v<0?-1.0:v>0?1.0:0.0;
}


void MarginTradingSvc::reportTrade(Value prevTrade, const TradeData &data) {

	Changeset wrtx = posDB.createChangeset();
	target->reportTrade(prevTrade, data);

	Guard _(lock);

	lastPrice = data.price;

	auto balanceChange = [&](const UserInfo &user, double multp){
		if (user.context.getString() == OrderContext::strMargin) {
			String docUserId  = user2docid(user.userId);
			Document doc = positionMap[Value(docUserId)];
			doc.setID(Value(docUserId));
			double prevPos = doc["position"].getNumber();
			double prevValue = doc["value"].getNumber();
				double assetChange = data.size * multp;
				double currChange = -data.price * data.size * multp;
				double newPos = mcfg->adjustSize(prevPos+assetChange);
				double newValue = mcfg->adjustTotal(prevValue+currChange);
			if (sign(newPos) != sign(prevPos)) newValue = mcfg->adjustPrice(-newPos * lastPrice);
			doc("position", newPos);
			doc("value", newValue);
			doc("lastTrade",data.id);
			doc.enableTimestamp();
			wrtx.update(doc);
		}
	};


	balanceChange(data.buyer,1);
	balanceChange(data.seller,-1);

	wrtx.commit();
	auto &commited = wrtx.getCommitedDocs();
	for (auto &&c : commited) {
		Value newDoc = c.doc.replace("_rev", c.newRev);
		positionMap[newDoc["_id"]] = newDoc;
	}

}


void MarginTradingSvc::syncPositions() {
	Guard _(lock);

	positionMap.clear();
	Query q = posDB.createQuery(View::includeDocs);
	q.prefixString("p.");
	Result res = q.exec();
	for (Row rw : res) {
		positionMap[rw.doc["_id"]] = rw.doc;
	}
}

void MarginTradingSvc::resync() {
	target->resync();
	syncPositions();

}

void MarginTradingSvc::updatePosition(Document doc) {
	posDB.put(doc);
	Guard _(lock);
	positionMap[doc.getIDValue()] = doc;

}

} /* namespace quark */
