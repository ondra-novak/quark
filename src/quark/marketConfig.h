#pragma once
#include <imtjson/value.h>
#include <imtjson/string.h>



namespace quark {

class MarketConfig: public json::RefCntObj {
public:


	MarketConfig(json::Value v);

	/// size of 1 pip (0.01 means that smallest step is 0.01)
	double currencyStep;
	/// order size granuality
	double assetStep;

	///minimum price allowed as limit/stop price
	double currencyMin;
	///maximum price allowed as limit/stop price
	double currencyMax;
	///minimum order size
	double assetMin;
	///maximum order size
	double assetMax;
	///maximum total budget for the order (maxPrice * maxSize)
	double budgetMax;

	///maximum allowed spread in per cents, if spread is larger, market orders are paused
	double maxSpreadPct;
	///maximum slippage for stop commands in ptc
	double maxSlippagePct;


	std::size_t priceToCurrency(double price) const;
	double currencyToPrice(std::size_t pip) const;

	std::size_t assetToSize(double amount) const;
	double sizeToAsset(std::size_t size) const;

	std::size_t budgetToFixPt(double budget)const ;
	double budgetFromFixPt(std::size_t pip)const;


	double adjustSize(double size)const;
	double adjustPrice(double price)const;
	double adjustTotal(double price)const;


	json::String currencySign;
	json::String assetSign;
	json::Value moneyService;
	json::String updateUrl;
	json::Value updateLastModified;
	json::Value updateETag;
	json::Value rev;

};

typedef json::RefCntPtr<MarketConfig> PMarketConfig;

} /* namespace quark */
