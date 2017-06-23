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
	:asset(0),currency(0),marginLong(0),marginShort(0),posLong(0),posShort(0)

{

}


Value OrderBudget::toJson() const {
	return Object("asset",asset)
			("currency",currency)
			("marginLong",marginLong)
			("marginShort",marginShort)
			("posShort",posShort)
			("posLong", posLong);
}


OrderBudget OrderBudget::operator +(const OrderBudget& other) const {
	return OrderBudget(asset+other.asset,
			currency + other.currency,
			marginLong + other.marginLong,
			marginShort + other.marginShort,
			posLong + other.posLong,
			posShort + other.posShort);
}
OrderBudget::OrderBudget(double asset,double currency,double marginLong, double marginShort,double posLong, double posShort)
:asset(asset),currency(currency),marginLong(marginLong),marginShort(marginShort),posLong(posLong),posShort(posShort) {}


OrderBudget::OrderBudget(double marginLong, double marginShort,double posLong, double posShort)
	:asset(0),currency(0),marginLong(marginLong),marginShort(marginShort),posLong(posLong),posShort(posShort) {}
OrderBudget::OrderBudget(double asset,double currency)
	:asset(asset),currency(currency),marginLong(0),marginShort(0),posLong(0),posShort(0) {}

static double adjustAssets(double asset, const MarketConfig& cfg)  {
	return asset < 0 ? 0 : (floor(asset / cfg.granuality) * cfg.granuality);
}

static double adjustCurrency(double currency , const MarketConfig& cfg) {
	return currency<0?0:(floor(currency/cfg.granuality/cfg.pipSize)*cfg.granuality*cfg.pipSize);
}

OrderBudget OrderBudget::adjust(const MarketConfig &cfg) const {
	return OrderBudget(adjustAssets(asset, cfg),
			adjustCurrency(currency,cfg),
			adjustCurrency(marginLong,cfg),
			adjustCurrency(marginShort,cfg),
			adjustAssets(posLong,cfg),
			adjustAssets(posShort,cfg));
}


} /* namespace quark */
