#pragma once

#include <couchit/couchDB.h>

#include "imoneyservice.h"
#include "orderBudget.h"

namespace quark {

using namespace json;
using namespace couchit;

class PositionTracker: public IMoneyService {
public:
	PositionTracker(CouchDB &posDB);

	virtual bool allocBudget(json::Value user, json::Value order, const OrderBudget &budget, Callback callback);
	virtual Value reportTrade(Value prevTrade, Value id, double price, double size, OrderDir::Type dir, std::size_t timestamp);
	virtual bool reportBalanceChange(Value trade, Value user, OrderContext::Type context, double assetChange, double currencyChange, double fee);

protected:
	CouchDB &posDB;
};

} /* namespace quark */


