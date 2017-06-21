#pragma once
#include <imtjson/refcnt.h>
#include <imtjson/value.h>

#include "../quark_lib/constants.h"


namespace quark {

using namespace json;


class OrderBudget;

class IMoneyService:public json::RefCntObj  {
public:
	virtual ~IMoneyService() {}


	typedef std::function<void(bool)> Callback;

	///Allocate user's budget
	/**
	 * @param user user identification
	 * @param order order identification
	 * @param budget budget information
	 * @param callback function called when allocation is complete.
	 *
	 * @note if function decides to access the money server, the response can be called
	 * asynchronously anytime later. If the request can be processed immediatelly, or
	 * without need to wait for the money server, the response is called also immediately.
	 *
	 * The argument of the response is true=budget allocated, false=allocation rejected
	 */

	virtual bool allocBudget(json::Value user, json::Value order, const OrderBudget &budget, Callback callback) = 0;

	///Report trade executed
	/**
	 * @param prevTrade ID of previous trade. If it doesn't match, function reports nothing and sends known last trade
	 * @param id id of current trade
	 * @param price price of the trade
	 * @param size size of trade (amount)
	 * @param dir taker's action (buy or sell)
	 * @param timestamp unix timestamp of the trade (may duplicate)
	 * @return if trade is reported, function returns id. If not, function retuns last known trade. Function
	 * can return null to start reporting from the beginning
	 */
	virtual Value reportTrade(Value prevTrade, Value id, double price, double size, OrderDir::Type dir, std::size_t timestamp) = 0;
	///Reports balance change for the user
	/**
	 * @param trade ID of trade as reference
	 * @param user ID of user
	 * @param context context (exchange or margin)
	 * @param assetChange (asset change - position change for margin)
	 * @param currencyChange (change of currency - or value of the position for margin trading)
	 * @param fee fee
	 * @retval true reported
	 * @retval false referenced trade was not last trade, please start over
	 */
	virtual bool reportBalanceChange(Value trade, Value user, OrderContext::Type context, double assetChange, double currencyChange, double fee) = 0;

};



}
