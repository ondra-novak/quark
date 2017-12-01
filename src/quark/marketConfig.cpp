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

MarketConfig::MarketConfig(json::Value v) {

	//new version
	if (v["version"].getUInt() == 2) {

		double maxNumber = pow(2,63)-1;

		Value asset = v["asset"];
		Value currency = v["currency"];
		currencyStep = currency["step"].getNumber();
		assetStep = asset["step"].getNumber();

		if (currencyStep<=0.0)
			throw std::runtime_error("MarketConfig/currency/step missing or invalid");
		if (assetStep<=0.0)
			throw std::runtime_error("MarketConfig/asset/step missing or invalid");

		double currencyTotalMax = maxNumber * currencyStep;
		double assetTotalMax = maxNumber * assetStep;
		double budgetTotalMax = maxNumber * assetStep * currencyStep;

		Value x = asset["min"];
		if (x.defined() && x.getNumber() > assetStep) assetMin = x.getNumber(); else assetMin = assetStep;
		x = asset["max"];
		if (x.defined() && x.getNumber() < assetTotalMax) assetMax = x.getNumber(); else assetMax = assetTotalMax;
		x = currency["min"];
		if (x.defined() && x.getNumber() > currencyStep) currencyMin = x.getNumber(); else currencyMin = currencyStep;
		x = currency["max"];
		if (x.defined() && x.getNumber() < currencyTotalMax) currencyMax = x.getNumber(); else currencyMax = currencyTotalMax;
		x = v["budgetMax"];
		if (x.defined() && x.getNumber() < budgetTotalMax) budgetMax = x.getNumber(); else budgetMax = budgetTotalMax;
		x = v["maxSpreadPct"];
		if (x.defined() && x.getNumber() >0) maxSpreadPct = x.getNumber(); else maxSpreadPct = 5;
		x = v["maxSlippagePct"];
		if (x.defined() && x.getNumber() >=0 && x.type() == json::number) maxSlippagePct = x.getNumber(); else maxSlippagePct = 5;
		x = asset["sign"];
		if (x.defined() && !x.getString().empty()) assetSign = String(x);
		else throw std::runtime_error("MarketConfig/asset/sign is mandatory");
		x = currency["sign"];
		if (x.defined() && !x.getString().empty()) currencySign = String(x);
		else throw std::runtime_error("MarketConfig/currency/sign is mandatory");


	} else{

		currencyStep = mandatory(v,"pipSize").getNumber();
		/// order size granuality
		assetStep = mandatory(v,"granuality").getNumber();

		///minimum price allowed as limit/stop price
		currencyMin = mandatory(v,"minPrice").getNumber();
		///maximum price allowed as limit/stop price
		currencyMax =mandatory(v,"maxPrice").getNumber();
		///minimum order size
		assetMin = mandatory(v,"minSize").getNumber();
		///maximum order size
		assetMax = mandatory(v,"maxSize").getNumber();
		///maximum total budget for the order (maxPrice * maxSize)
		budgetMax = mandatory(v,"maxBudget").getNumber();

		///maximum allowed spread in per cents, if spread is larger, market orders are paused
		maxSpreadPct = mandatory(v,"maxSpreadPct").getNumber();
		///maximum allowed spread in per cents, if spread is larger, market orders are paused
		maxSlippagePct = mandatory(v,"maxSlippagePct").getNumber();

		currencySign = String(mandatory(v,"currencySign"));
		assetSign = String(mandatory(v,"assetSign"));


		double maxSize = pow(2,sizeof(std::size_t)*8);
		double reqSize = std::min(currencyMax * maxSize, budgetMax) / currencyStep / assetStep;
		if (reqSize >= maxSize) throw std::runtime_error("maxPrice * maxSize or maxBudget is out of range to map to the numbers");

	}


	moneyService = mandatory(v,"moneyService");
	updateUrl = String(v["updateUrl"]);
	updateLastModified = String(v["updateLastModified"]);
	updateETag = String(v["updateETag"]);
	rev = v["_rev"];


}





std::size_t quark::MarketConfig::priceToCurrency(double price) const {
	return (std::size_t)floor(price / currencyStep+0.5);
}

double quark::MarketConfig::currencyToPrice(std::size_t pip) const {
	return pip * currencyStep;
}

std::size_t quark::MarketConfig::assetToSize(double amount) const {
	return (std::size_t)floor(amount / assetStep+0.5);
}

double quark::MarketConfig::sizeToAsset(std::size_t size) const {
	return size * assetStep;
}

std::size_t MarketConfig::budgetToFixPt(double budget) const {
	return (std::size_t)floor(budget / currencyStep / assetStep+0.5);
}

double MarketConfig::budgetFromFixPt(std::size_t pip) const {
	return pip * currencyStep * assetStep;
}

double quark::MarketConfig::adjustSize(double size) const {
	return floor(size/assetStep+0.5)*assetStep;
}

double quark::MarketConfig::adjustPrice(double price) const {
	return floor(price/currencyStep+0.5)*currencyStep;
}
double quark::MarketConfig::adjustTotal(double price) const {
	double comb = currencyStep * assetStep;
	return floor(price/comb+0.5)*comb;
}


} /* namespace quark */

