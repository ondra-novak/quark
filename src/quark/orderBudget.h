#pragma once
#include <imtjson/value.h>
#include <imtjson/namedEnum.h>
#include "../quark_lib/constants.h"

namespace quark {

class MarketConfig;
using namespace json;

class OrderBudget {
public:

	///Blocked asset (sum of assets for sell commands)
	double asset;
	///Blocked currency (sum of currencies for buy commands)
	double currency;
	///Blocked margin (leverage is not applied)
	double margin;
	///Total margin assets blocked for short positions
	double posShort;
	///Total margin assets blocked for long positions
	double posLong;


	OrderBudget();
	OrderBudget(double asset,double currency);
	OrderBudget(double margin,double posShort, double posLong);
	OrderBudget(double asset,double currency,double margin,double posShort, double posLong);
	Value toJson() const;

	OrderBudget operator+(const OrderBudget &other) const;
	OrderBudget adjust(const MarketConfig &cfg) const;
	bool operator==(const OrderBudget &b) const {
		return asset == b.asset
				&& currency == b.currency
				&& margin == b.margin;
	}
	bool operator!=(const OrderBudget &b) const {
		return !operator==(b);
	}

	bool above(const OrderBudget &b) const {
		return asset > b.asset
				|| currency > b.currency
				|| margin > b.margin;

	}


};

} /* namespace quark */

