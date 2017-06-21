#pragma once
#include <imtjson/value.h>
#include <imtjson/namedEnum.h>
#include "../quark_lib/constants.h"

namespace quark {

class MarketConfig;
using namespace json;

class OrderBudget {
public:

	enum Type {
		currency,
		asset
	};

	static NamedEnum<Type> str;

	OrderContext::Type context;
	Type type;
	double value;

	OrderBudget();
	OrderBudget(OrderContext::Type context, Type type, double value);
	Value toJson() const;

	OrderBudget operator-(const OrderBudget &other) const;
	OrderBudget operator+(const OrderBudget &other) const;
	OrderBudget adjust(const MarketConfig &cfg) const;
	OrderBudget operator-() const;
	bool operator==(const OrderBudget &b) const {
		return type == b.type && value == b.value;
	}
	bool operator!=(const OrderBudget &b) const {
		return !operator==(b);
	}
	bool operator>(const OrderBudget &b) const {
		return value > b.value;
	}
	bool operator<(const OrderBudget &b) const {
		return value < b.value;
	}
	bool operator>=(const OrderBudget &b) const {
		return value >= b.value;
	}
	bool operator<=(const OrderBudget &b) const {
		return value <= b.value;
	}


};

} /* namespace quark */

