#pragma once
#include <imtjson/value.h>
#include <imtjson/string.h>



namespace quark {

class MarketConfig: public json::RefCntObj {
public:
	MarketConfig(json::Value v);

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
	///maximum slippage for stop commands in ptc
	double maxSlippagePtc;


	std::size_t priceToPip(double price) const;
	double pipToPrice(std::size_t pip) const;

	std::size_t amountToSize(double amount) const;
	double sizeToAmount(std::size_t size) const;

	std::size_t budgetToPip(double budget)const ;
	double pipToBudget(std::size_t pip)const;


	double adjustSize(double size)const;
	double adjustPrice(double price)const;
	double adjustTotal(double price)const;


	json::String signature;
	json::String currencySign;
	json::String assetSign;

};

typedef json::RefCntPtr<MarketConfig> PMarketConfig;

} /* namespace quark */
