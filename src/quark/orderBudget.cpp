/*
 * blockedBudget.cpp
 *
 *  Created on: Jun 4, 2017
 *      Author: ondra
 */

#include "orderBudget.h"

#include <imtjson/object.h>
#include <cmath>


#include "marketConfig.h"

namespace quark {


OrderBudget::OrderBudget()
	:asset(0),currency(0),margin(0),posShort(0),posLong(0)

{

}


Value OrderBudget::toJson() const {
	return Object("asset",asset)
			("currency",currency)
			("margin",margin)
			("posShort",posShort)
			("posLong", posLong);
}


OrderBudget OrderBudget::operator +(const OrderBudget& other) const {
	return OrderBudget(asset+other.asset,
			currency + other.currency,
			margin + other.margin,
			posShort + other.posShort,
			posLong + other.posLong);
}
OrderBudget::OrderBudget(double asset,double currency,double margin,double posShort, double posLong)
:asset(asset),currency(currency),margin(margin),posShort(posShort),posLong(posLong) {}


OrderBudget::OrderBudget(double margin,double posShort, double posLong)
	:asset(0),currency(0),margin(margin),posShort(posShort),posLong(posLong) {}
OrderBudget::OrderBudget(double asset,double currency)
	:asset(asset),currency(currency),margin(0),posShort(0),posLong(0) {}

static double adjustAssets(double asset, const MarketConfig& cfg)  {
	return asset < 0 ? 0 : (floor(asset / cfg.granuality) * cfg.granuality);
}

static double adjustCurrency(double currency , const MarketConfig& cfg) {
	return currency<0?0:(floor(currency/cfg.granuality/cfg.pipSize)*cfg.granuality*cfg.pipSize);
}

OrderBudget OrderBudget::adjust(const MarketConfig &cfg) const {
	return OrderBudget(adjustAssets(asset, cfg),
			adjustCurrency(currency,cfg),
			adjustCurrency(margin,cfg),
			adjustAssets(posShort,cfg),
			adjustAssets(posLong,cfg));
}


} /* namespace quark */
