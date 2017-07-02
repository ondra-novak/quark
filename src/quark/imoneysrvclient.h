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

	enum AllocResult {
		allocOk,
		allocReject,
		allocError,
		allocTryAgain
	};


	typedef std::function<void(AllocResult)> Callback;


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
		bool taker;
	};

	///Report trade executed
	/**
	 * @param prevTrade ID of previous trade. If it doesn't match, function must perform resync or report an error
	 * @param data trade data
	 */
	virtual void reportTrade(Value prevTrade, const TradeData &data) = 0;
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
	virtual void reportBalanceChange(const BalanceChange &data) = 0;

	virtual void commitTrade(Value tradeId) = 0;





};




typedef RefCntPtr<IMoneySrvClient> PMoneySrvClient;

///Support interface for moneyserver clients
/**
 * Contains various function need by moneyserver clients
 */
class IMoneySvcSupport : public RefCntObj {
public:
	///Perform resync trades for the target from given trade to given trade
	/**
	 * @param target target money server
	 * @param fromTrade from tradeId
	 * @param toTrade to tradeId
	 */
	virtual void resync(PMoneySrvClient target, const Value fromTrade, const Value toTrade) = 0;
	///Dispatch function to internal dispatcher
	/**
	 * Send function through the dispatcher. It should run in different thread or in the
	 * same thread but after this code is finished
	 * @param fn function to run
	 */
	virtual void dispatch(std::function<void()> fn) = 0;
};

typedef RefCntPtr<IMoneySvcSupport> PMoneySvcSupport;


}
