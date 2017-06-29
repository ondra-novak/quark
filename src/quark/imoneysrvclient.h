#pragma once
#include <imtjson/refcnt.h>
#include <imtjson/value.h>

#include "../quark_lib/constants.h"
#include "marketConfig.h"


namespace quark {

using namespace json;


class OrderBudget;

class IMoneySrvClient:public json::RefCntObj  {
public:
	virtual ~IMoneySrvClient() {}

	typedef std::function<void(bool)> Callback;


	///Adjusts budget - mostly used during margin trading
	virtual void adjustBudget(json::Value user, OrderBudget &budget) = 0;

	///Request budget allocation directly on server
	/**
	 * @param user user ID
	 * @param total absolute budget for given context
	 * @param callback callback function. If set to nullptr, no callback is required
	 * @retval true operation succesed without need to access the server
	 * @retval false operation require access to the server and runs asynchronously, callback will be called if defined
	 *
	 */
	virtual bool allocBudget(json::Value user, OrderBudget total, Callback callback) = 0;




	struct TradeData {
		Value id;
		double price;
		double size;
		std::size_t nonce;
		OrderDir::Type dir;
		std::size_t timestamp;
	};

	struct BalanceChange {
		Value trade;
		Value user;
		OrderContext::Type context;
		double assetChange;
		double currencyChange;
		double fee;
	};

	///Report trade executed
	/**
	 * @param prevTrade ID of previous trade. If it doesn't match, function reports nothing and sends known last trade
	 * @param data trade data
	 * @return if trade is reported, function returns id. If not, function retuns last known trade. Function
	 * can return null to start reporting from the beginning
	 */
	virtual Value reportTrade(Value prevTrade, const TradeData &data) = 0;
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
	virtual bool reportBalanceChange(const BalanceChange &data) = 0;

	virtual void commitTrade(Value tradeId) = 0;





};

typedef RefCntPtr<IMoneySrvClient> PMoneySrvClient;

}
