#pragma once
#include <imtjson/json.h>


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


	enum Dir {
		buy,
		sell
	};

	enum Type {
		market,
		limit,
		postlimit,
		stop,
		stoplimit,
		fok,
		ioc,
		trailingStop,
		trailingStopLimit,
		trailingLimit
	};

	enum State {
		prepared,
		///Order is located in market queue. The queue pos defines its location
		marketQueue,
		///Order is located in the orderbook
		orderbook,
		///Order is located in the stop queue
		stopQueue,
	};

	typedef json::Value Value;

	Dir getDir() const {
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

	Type getType() const {
		return type;
	}


	State getState() const {
		return state;
	}


	POrder changeState(State newState) const;
	POrder changeType(Type newType) const;
	POrder updateAfterTrade(std::size_t price, std::size_t size);
	///determines, whether the order will be filled complete when trade operation executes on give price and size
	bool willBeFilled(std::size_t price, std::size_t size) const;

	bool isSimpleUpdate(const Order &other) const;
	POrder doSimpleUpdate(const Order &other) const;
	POrder updateTrailing(std::size_t newPrice) const;
protected:

	Value id;
	std::size_t size;
	std::size_t budget; //total buy/sell budget - applied only on unlimited market and stop trades
	std::size_t limitPrice;
	std::size_t triggerPrice;
	std::size_t trailingDistance;
	int domPriority;
	int queuePriority;
	std::size_t queuePos;
	Dir dir;
	Type type;
	State state;


};

typedef json::RefCntPtr<Order> POrder;

} /* namespace quark */


