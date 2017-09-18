#pragma once
#include <imtjson/refcnt.h>
#include <imtjson/value.h>

#include "../quark_lib/constants.h"
#include "marketConfig.h"

class Dispatcher;

namespace quark {

using namespace json;


class OrderBudget;


class ITradeStream {
public:

	struct UserInfo {
		Value userId;
		Value context;
	};

	struct TradeData {
		Value id;
		double price;
		double size;
		OrderDir::Type dir;
		std::size_t timestamp;
		UserInfo buyer;
		UserInfo seller;

	};

	virtual void reportTrade(Value prevTrade, const TradeData &data) = 0;

	virtual ~ITradeStream() {}
};


///Support interface - client can call these functions
class IMoneySrvClientSupport {
public:
	virtual ~IMoneySrvClientSupport() {}

	///resync trades
	virtual void resync(ITradeStream &target, const Value fromTrade, const Value toTrade) = 0;
	///cancel all orders of given users
	virtual bool cancelAllOrders(const json::Array &users) = 0;
	///Receive dispatcher
	virtual Dispatcher &getDispatcher() = 0;

};

class IMoneySrvClient: public ITradeStream, public json::RefCntObj  {
public:

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

};




typedef RefCntPtr<IMoneySrvClient> PMoneySrvClient;



}
