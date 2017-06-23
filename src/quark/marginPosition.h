#pragma once

#include <couchit/couchDB.h>
#include <couchit/document.h>
#include <unordered_map>


#include "imoneyservice.h"
#include "orderBudget.h"

namespace quark {

using namespace json;
using namespace couchit;

class PositionTracker: public IMoneyService {
public:
	PositionTracker(CouchDB &posDB, PMoneyService target);

	virtual bool allocBudget(json::Value user, json::Value order, const OrderBudget &budget, Callback callback);
	virtual Value reportTrade(Value prevTrade, Value id, double price, double size, OrderDir::Type dir, std::size_t timestamp);
	virtual bool reportBalanceChange(Value trade, Value user, OrderContext::Type context, double assetChange, double currencyChange, double fee);

	void syncPositions();
	void updatePosition(Document doc);

protected:
	CouchDB &posDB;
	PMoneyService target;
	double lastPrice;
	Value prevTrade;


	typedef std::unordered_map<Value, Value> PositionMap;
	PositionMap positionMap;


};

} /* namespace quark */


