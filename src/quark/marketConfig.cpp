/*
 * markedConfig.cpp
 *
 *  Created on: Jun 3, 2017
 *      Author: ondra
 */

#include "marketConfig.h"

#include <imtjson/json.h>

#include <stdexcept>

namespace quark {

using namespace json;

static Value mandatory(json::Value v, StrViewA name) {
	Value x = v[name];
	if (x.defined()) return x;
	else throw std::runtime_error(String({"Config value: ", name, " is mandatory, but missing"}).c_str());
}

MarketConfig::MarketConfig(json::Value v)
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
///maximum allowed spread in per cents, if spread is larger, market orders are paused
,maxSlippagePtc(mandatory(v,"maxSlippagePtc").getNumber())

,signature(mandatory(v,"signature"))
,currencySign(mandatory(v,"currencySign"))
,assetSign(mandatory(v,"assetSign"))
{

	double maxSize = pow(2,sizeof(std::size_t)*8);
	double reqSize = std::min(maxPrice * maxSize, maxBudget) / pipSize / granuality;
	if (reqSize >= maxSize) throw std::runtime_error("maxPrice * maxSize or maxBudget is out of range to map to the numbers");

}


std::size_t quark::MarketConfig::priceToPip(double price) {
	return (std::size_t)floor(price / pipSize);
}

double quark::MarketConfig::pipToPrice(std::size_t pip) {
	return pip * pipSize;
}

std::size_t quark::MarketConfig::amountToSize(double amount) {
	return (std::size_t)floor(amount / granuality+0.5);
}

double quark::MarketConfig::sizeToAmount(std::size_t size) {
	return size * granuality;
}

std::size_t MarketConfig::budgetToPip(double budget) {
	return (std::size_t)floor(budget / pipSize / granuality+0.5);
}

double MarketConfig::pipToBudget(std::size_t pip) {
	return pip * pipSize * granuality;
}

} /* namespace quark */
