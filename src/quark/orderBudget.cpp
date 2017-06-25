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


OrderBudget OrderBudget::adjust(const MarketConfig &cfg) const {
	return OrderBudget(cfg.adjustSize(asset),
			cfg.adjustPrice(currency),
			cfg.adjustPrice(marginLong),
			cfg.adjustPrice(marginShort),
			cfg.adjustSize(posLong),
			cfg.adjustSize(posShort));
}


} /* namespace quark */
