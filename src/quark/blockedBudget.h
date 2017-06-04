#pragma once
#include <imtjson/value.h>

namespace quark {

class MarketConfig;
using namespace json;

class BlockedBudget {
public:

	double assets;
	double currency;
	BlockedBudget();
	BlockedBudget(double assets, double currency);
	BlockedBudget(Value fromjson);
	Value toJson() const;

	BlockedBudget operator-(const BlockedBudget &other) const;
	BlockedBudget operator+(const BlockedBudget &other) const;
	BlockedBudget adjust(const MarketConfig &cfg) const;
	BlockedBudget operator-() const;


};

} /* namespace quark */

