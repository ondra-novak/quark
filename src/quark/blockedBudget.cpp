/*
 * blockedBudget.cpp
 *
 *  Created on: Jun 4, 2017
 *      Author: ondra
 */

#include "blockedBudget.h"

#include <imtjson/object.h>
#include <cmath>


#include "marketConfig.h"

namespace quark {

BlockedBudget::BlockedBudget():assets(0),currency(0) {

}

BlockedBudget::BlockedBudget(Value fromjson)
	:assets(fromjson["assets"].getNumber())
	,currency(fromjson["currency"].getNumber())
{
}

Value BlockedBudget::toJson() const {
	return Object("assets",assets)
			("currency",currency);
}

BlockedBudget BlockedBudget::operator -(const BlockedBudget& other) const {
	BlockedBudget b;
	b.assets = assets - other.assets;
	b.currency = currency - other.currency;
	return b;
}

BlockedBudget BlockedBudget::operator +(const BlockedBudget& other) const {
	BlockedBudget b;
	b.assets = assets + other.assets;
	b.currency = currency + other.currency;
	return b;
}

BlockedBudget::BlockedBudget(double assets, double currency)
	:assets(assets),currency(currency)
{
}

BlockedBudget BlockedBudget::adjust(const MarketConfig &cfg) const {
	return BlockedBudget(
		assets<0?0:(floor(assets / cfg.granuality)*cfg.granuality),
		currency<0?0:(floor(currency/cfg.granuality/cfg.pipSize)*cfg.granuality*cfg.pipSize));

}

BlockedBudget BlockedBudget::operator -() const {
	return BlockedBudget(-assets,-currency);
}

} /* namespace quark */
