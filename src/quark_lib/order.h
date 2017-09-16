#pragma once
#include <imtjson/json.h>

#include "constants.h"


namespace quark {

class Order;

typedef json::RefCntPtr<Order> POrder;

class RefCntObj: public json::RefCntObj {
public:
	RefCntObj() {}
	RefCntObj(const RefCntObj &) {}

};

struct OrderJsonData {

	json::Value id;
	json::Value user;
	json::Value data;
	json::String dir;
	json::String type;
	std::size_t size;
	std::size_t limitPrice;
	std::size_t stopPrice;
	std::size_t trailingDistance;
	std::size_t budget;
	int queuePriority;
	int domPriority;

	OrderJsonData(json::Value v);
	OrderJsonData() {}

};

class Order: public RefCntObj {
public:
	Order();
	Order(const OrderJsonData &data);




	enum State {
		prepared,
		///Order is located in market queue. The queue pos defines its location
		marketQueue,
		///Order is located in the orderbook
		orderbook,
		///Order is located in the stop queue
		stopQueue,
		///Order is oco and it is located in both queues
		oco,
	};

	typedef json::Value Value;

	OrderDir::Type getDir() const {
		return dir;
	}

	int getDomPriority() const {
		return domPriority;
	}

	Value getId() const {
		return id;
	}

	std::size_t getLimitPrice() const {
		return limitPrice;
	}

	std::size_t getQueuePos() const {
		return queuePos;
	}

	int getQueuePriority() const {
		return queuePriority;
	}

	std::size_t getSize() const {
		return size;
	}

	std::size_t getSizeAtPrice(std::size_t price) const;


	std::size_t getTriggerPrice() const {
		return triggerPrice;
	}

	OrderType::Type getType() const {
		return type;
	}


	State getState() const {
		return state;
	}

	Value getUser() const {
		return user;
	}

	bool isTrailing() const {
		return trailingDistance != 0;
	}

	POrder changeState(State newState) const;
	POrder changeType(OrderType::Type newType) const;
	POrder updateAfterTrade(std::size_t price, std::size_t size);
	///determines, whether the order will be filled complete when trade operation executes on give price and size
	bool willBeFilled(std::size_t price, std::size_t size) const;

	bool isSimpleUpdate(const Order &other) const;
	POrder doSimpleUpdate(const Order &other) const;
	POrder updateTrailing(std::size_t newPrice) const;
	std::intptr_t calcTrailingMove(std::size_t refPrice, std::size_t newPrice, bool rev) const;

	const Value& getData() const {
		return data;
	}


protected:

	Value id;
	Value user;
	Value data;
	std::size_t size;
	std::size_t budget; //total buy/sell budget - applied only on unlimited market and stop trades
	std::size_t limitPrice;
	std::size_t triggerPrice;
	std::size_t trailingDistance;
	int domPriority;
	int queuePriority;
	std::size_t queuePos;
	OrderDir::Type dir;
	OrderType::Type type;
	State state;


};

typedef json::RefCntPtr<Order> POrder;

} /* namespace quark */


