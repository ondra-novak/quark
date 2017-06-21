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
	return OrderBudget(context, type, value - other.value);
}

OrderBudget OrderBudget::operator +(const OrderBudget& other) const {
	return OrderBudget(context, type, value + other.value);
}

OrderBudget::OrderBudget(OrderContext::Type context,Type type, double value)
	:context(context),type(type),value(value)
{
}

OrderBudget OrderBudget::adjust(const MarketConfig &cfg) const {
	if (type == asset)
		return OrderBudget(context, type,value<0?0:(floor(value / cfg.granuality)*cfg.granuality));
	else
		return OrderBudget(context, type,value<0?0:(floor(value/cfg.granuality/cfg.pipSize)*cfg.granuality*cfg.pipSize));

}

OrderBudget OrderBudget::operator -() const {
	return OrderBudget(context, type,-value);
}

} /* namespace quark */
