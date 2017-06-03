#pragma once
#include <imtjson/value.h>

namespace quark {

class MarkedConfig {
public:
	MarkedConfig(json::Value v);

	/// size of 1 pip (0.01 means that smallest step is 0.01)
	double pipSize;
	/// order size granuality
	double granuality;

	///minimum price allowed as limit/stop price
	double minPrice;
	///maximum price allowed as limit/stop price
	double maxPrice;
	///minimum order size
	double minSize;
	///maximum order size
	double maxSize;
	///maximum total budget for the order (maxPrice * maxSize)
	double maxBudget;

	///maximum allowed spread in per cents, if spread is larger, market orders are paused
	double maxSpreadPtc;

};

} /* namespace quark */
