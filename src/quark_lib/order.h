#pragma once
#include <imtjson/json.h>


namespace quark {

class Order: public json::RefCntObj {
public:
	Order();
	Order(json::Value data);

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
		ioc
	};

	enum State {
		///Order is located in market queue. The queue pos defines its location
		marketQueue,
		///Order is located in the orderbook
		orderbook,
		///Order is located in the stop queue
		stopQueue
	};

	typedef json::Value Value;

	Value id;
	std::size_t size;
	std::size_t limitPrice;
	std::size_t triggerPrice;
	int domPriority;
	int queuePriority;
	std::size_t queuePos;
	Dir dir;
	Type type;


};

typedef json::RefCntPtr<Order> POrder;

} /* namespace quark */


