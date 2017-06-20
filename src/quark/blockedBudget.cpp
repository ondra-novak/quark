/*
 * blockedBudget.cpp
 *
 *  Created on: Jun 4, 2017
 *      Author: ondra
 */

#include "blockedBudget.h"

#include <imtjson/object.h>
#include <cmath>


#include "marketConfig.h"

namespace quark {

NamedEnum<OrderBudget::Type> OrderBudget::str({
	{OrderBudget::asset,"asset"},
	{OrderBudget::currency,"currency"}
});

OrderBudget::OrderBudget():type(currency),value(0) {

}


Value OrderBudget::toJson() const {
	return Object("type",str[type])
			("value",value);
}

OrderBudget OrderBudget::operator -(const OrderBudget& other) const {
	return OrderBudget(type, value - other.value);
}

OrderBudget OrderBudget::operator +(const OrderBudget& other) const {
	return OrderBudget(type, value + other.value);
}

OrderBudget::OrderBudget(Type type, double value)
	:type(type),value(value)
{
}

OrderBudget OrderBudget::adjust(const MarketConfig &cfg) const {
	if (type == asset)
		return OrderBudget(type,value<0?0:(floor(value / cfg.granuality)*cfg.granuality));
	else
		return OrderBudget(type,value<0?0:(floor(value/cfg.granuality/cfg.pipSize)*cfg.granuality*cfg.pipSize));

}

OrderBudget OrderBudget::operator -() const {
	return OrderBudget(type,-value);
}

} /* namespace quark */
