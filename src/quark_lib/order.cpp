/*
 * order.cpp
 *
 *  Created on: 19. 5. 2017
 *      Author: ondra
 */

#include "order.h"

#include "orderErrorException.h"

namespace quark {

Order::Order() {}

Order::Order(json::Value data) {

	using namespace json;

	id = data["id"];
	if (!id.defined()) {
		throw OrderErrorException(id,OrderErrorException::orderHasNoID, "Order has no ID");
	}

	Value jd = data["dir"];
	if (jd.getString() == "buy") {
		dir = buy;
	} else if (jd.getString() == "sell") {
		dir = sell;
	} else {
		throw OrderErrorException(id, OrderErrorException::unknownDirection, "Uknown dir - The field 'dir' must be either 'buy' or 'sell'");
	}



	StrViewA tp = data["type"].getString();
	if (tp == "market") type = market;
	else if (tp == "limit") type = limit;
	else if (tp == "stop") type = stop;
	else if (tp == "stoplimit") type = stoplimit;
	else if (tp == "postlimit") type = postlimit;
	else if (tp == "ioc") type = ioc;
	else if (tp == "fok") type = fok;
	else if (tp == "trailing-stop") type = trailingStop;
	else if (tp == "trailing-stoplimit") type = trailingStopLimit;
	else if (tp == "trailing-limit") type = trailingLimit;
	else
		throw OrderErrorException(id,OrderErrorException::orderTypeNotSupported, String({"Order type '", tp, "' is not supported"}).c_str());


	size = data["size"].getUInt();
	limitPrice = data["limitPrice"].getUInt();
	triggerPrice = data["stopPrice"].getUInt();
	trailingDistance = data["trailingDistance"].getUInt();
	if (size == 0)
		throw OrderErrorException(id, OrderErrorException::invalidOrMissingSize, "Invalid or missing 'size'");
	budget = data["budget"].getUInt();


	if (limitPrice == 0 && (type == limit
			|| type == stoplimit
			|| type == fok
			|| type == ioc
			|| type == postlimit
			|| type == trailingLimit
			|| type == trailingStopLimit
		)) {
		throw OrderErrorException(id, OrderErrorException::invalidOrMissingLimitPrice, "Invalid or missing 'limitPrice'");
	}

	if (triggerPrice == 0 && (type == stop
			|| type == stoplimit
			|| type == trailingStopLimit
			|| type == trailingStop
		)) {
		throw OrderErrorException(id, OrderErrorException::invalidOrMissingStopPrice, "Invalid or missing 'stopPrice'");
	}

	if (trailingDistance == 0 && (type == trailingLimit
			|| type == trailingStop
			|| type == trailingStopLimit))
	{
			throw OrderErrorException(id, OrderErrorException::invalidOrMissingTrailingDistance, "Invalid or missing 'trailingDistance'");
	}


	domPriority = data["domPriority"].getInt();
	queuePriority = data["queuePriority"].getInt();

}

static std::size_t counter = 0;

POrder quark::Order::changeState(State newState) const {

	Order *x = new Order(*this);
	x->state = newState;
	x->queuePos = counter++;
	return x;
}


POrder Order::updateAfterTrade(std::size_t price, std::size_t size) {
	if (size > this->size) throw std::runtime_error("Matched larger size then available");
	if (size == this->size) return nullptr;
	std::size_t total = 0;
	if (budget) {
		total = price *size;
		if (total > budget) throw std::runtime_error("Order's budget overrun");
		if (budget - total < price ) return nullptr;
	}
	Order *x = new Order(*this);
	x->size -= size;
	x->budget -= total;
	return x;



}

POrder Order::changeType(Type newType) const {
	Order *x = new Order(*this);
	x->type= newType;
	x->queuePos = counter++;
	return x;
}

bool quark::Order::isSimpleUpdate(const Order& other) const {
	return id == other.id
			&& limitPrice == other.limitPrice
			&& type == other.type
			&& dir == other.dir;

}

POrder Order::doSimpleUpdate(const Order& other) const {
	Order *x = new Order(*this);
	x->id = id;
	x->triggerPrice = other.triggerPrice;
	x->size = other.size;
	x->domPriority = other.domPriority;
	x->queuePriority = other.queuePriority;
	x->budget = other.budget;
	return x;
}

std::size_t Order::getSizeAtPrice(std::size_t price) const {
	if (budget == 0) return size;
	std::size_t maxsize = budget/price;
	return std::min(maxsize,size);
}

POrder Order::updateTrailing(std::size_t newPrice) const {
	switch (type) {
	case trailingStop:
	case trailingStopLimit:
		if (dir == buy)
			if (newPrice + trailingDistance < triggerPrice) {
				Order *x = new Order(*this);
				x->triggerPrice = newPrice + trailingDistance;
				std::size_t diff = triggerPrice-x->triggerPrice;
				if (diff > x->limitPrice) x->limitPrice = 0;
				else x->limitPrice -= diff;
				return x;
			} else {
				return const_cast<Order *>(this);
			}
		else {
			if (triggerPrice + trailingDistance < newPrice) {
				Order *x = new Order(*this);
				x->triggerPrice = newPrice - trailingDistance;
				std::size_t diff = x->triggerPrice-triggerPrice;
				x->limitPrice += diff;
				return x;
			} else {
				return const_cast<Order *>(this);
			}
		}
		break;
	case trailingLimit:
		if (dir == sell)
			if (newPrice + trailingDistance < limitPrice) {
				Order *x = new Order(*this);
				x->limitPrice = newPrice + trailingDistance;
				return x;
			} else {
				return const_cast<Order *>(this);
			}
		else {
			if (limitPrice + trailingDistance < newPrice) {
				Order *x = new Order(*this);
				x->limitPrice = newPrice - trailingDistance;
				return x;
			} else {
				return const_cast<Order *>(this);
			}
		}
		break;
	default:
		break;
	}
	return const_cast<Order *>(this);
}

} /* namespace quark */

