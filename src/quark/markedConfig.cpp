/*
 * markedConfig.cpp
 *
 *  Created on: Jun 3, 2017
 *      Author: ondra
 */

#include "markedConfig.h"

#include <imtjson/json.h>

#include <stdexcept>

namespace quark {

using namespace json;

static Value mandatory(json::Value v, StrViewA name) {
	Value x = v[name];
	if (x.defined()) return x;
	else throw std::runtime_error(String({"Config value: ", name, " is mandatory, but missing"}).c_str());
}

MarkedConfig::MarkedConfig(json::Value v)
:pipSize(mandatory(v,"pipSize").getNumber())
/// order size granuality
,granuality(mandatory(v,"granuality").getNumber())

///minimum price allowed as limit/stop price
,minPrice(mandatory(v,"minPrice").getNumber())
///maximum price allowed as limit/stop price
,maxPrice(mandatory(v,"maxPrice").getNumber())
///minimum order size
,minSize(mandatory(v,"minSize").getNumber())
///maximum order size
,maxSize(mandatory(v,"maxSize").getNumber())
///maximum total budget for the order (maxPrice * maxSize)
,maxBudget(mandatory(v,"maxBudget").getNumber())

///maximum allowed spread in per cents, if spread is larger, market orders are paused
,maxSpreadPtc(mandatory(v,"maxSpreadPtc").getNumber())
{



}

} /* namespace quark */
