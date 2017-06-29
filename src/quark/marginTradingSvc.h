#pragma once

#include <unordered_map>
#include <mutex>
#include <couchit/changeset.h>
#include <couchit/couchDB.h>
#include <couchit/document.h>


#include "imoneysrvclient.h"
#include "orderBudget.h"

namespace quark {

using namespace json;
using namespace couchit;

class MarginTradingSvc: public IMoneySrvClient {
public:
	MarginTradingSvc(CouchDB &posDB, PMarketConfig mcfg, PMoneySrvClient target);

	virtual void adjustBudget(json::Value user, OrderBudget &budget) override;
	virtual bool allocBudget(json::Value user, OrderBudget total, Callback callback);

	virtual Value reportTrade(Value prevTrade, const TradeData &data);
	virtual bool reportBalanceChange(const BalanceChange &data);
	virtual void commitTrade(Value tradeId) ;


	void syncPositions();
	void updatePosition(Document doc);

protected:
	CouchDB &posDB;
	PMoneySrvClient target;
	double lastPrice;


	typedef std::unordered_map<Value, Value> PositionMap;
	PositionMap positionMap;

	Changeset wrtx;
	std::mutex lock;
	typedef std::unique_lock<std::mutex> Guard;

	PMarketConfig mcfg;



};

} /* namespace quark */


