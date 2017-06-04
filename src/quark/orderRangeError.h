#pragma once
#include "../quark_lib/orderErrorException.h"

namespace quark {

class OrderRangeError: public OrderErrorException {
public:
	OrderRangeError(json::Value orderId, int code, double rangeValue);

	double getRangeValue() const {return rangeValue;}

	static const int minOrderSize = 2000;
	static const int maxOrderSize = 2001;
	static const int minPrice = 2002;
	static const int maxPrice = 2003;
	static const int outOfAllowedBudget = 2004;
	static const int invalidBudget = 2005;

protected:
	double rangeValue;
};

}

