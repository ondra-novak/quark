/*
 * order.cpp
 *
 *  Created on: 19. 5. 2017
 *      Author: ondra
 */

#include "order.h"

#include "orderErrorException.h"
#include "../quark/logfile.h"

namespace quark {

static std::size_t pos_counter = 0;

Order::Order():active(false) {
	queuePos = pos_counter++;
}

void unhandledException();

Order::~Order() {
	if (active) {
		try {
			throw std::runtime_error(json::String({"Destroing active/lost order: ", this->id.toString()}).c_str());
		} catch (...) {
			unhandledException();
		}
	}

}


OrderJsonData::OrderJsonData(json::Value data) {

	using namespace json;

	id = data["id"];
	dir = String(data["dir"]);
	type = String(data["type"]);
	size = data["size"].getUInt();
	limitPrice = data["limitPrice"].getUInt();
	stopPrice = data["stopPrice"].getUInt();
	trailingDistance = data["trailingDistance"].getUInt();
	domPriority = data["domPriority"].getInt();
	queuePriority = data["queuePriority"].getInt();
	budget = data["budget"].getInt();

}

Order::Order(const OrderJsonData &data):active(true) {


	using namespace json;

	id = data.id;
	if (!id.defined()) {
		throw OrderErrorException(id,OrderErrorException::orderHasNoID, "Order has no ID");
	}

	Value jd = data.dir;
	dir = OrderDir::str[jd.getString()];

	StrViewA tp = data.type;
	type = OrderType::str[tp];

	size = data.size;
	this->data = data.data;
	user = data.user;
	limitPrice = data.limitPrice;
	triggerPrice = data.stopPrice;
	trailingDistance = data.trailingDistance;
	if (size == 0)
		throw OrderErrorException(id, OrderErrorException::invalidOrMissingSize, "Invalid or missing 'size'");
	budget = data.budget;


	if (limitPrice == 0 && (type == OrderType::limit
			|| type == OrderType::stoplimit
			|| type == OrderType::fok
			|| type == OrderType::ioc
			|| type == OrderType::maker
			|| type == OrderType::oco_limitstop
		)) {
		throw OrderErrorException(id, OrderErrorException::invalidOrMissingLimitPrice, "Invalid or missing 'limitPrice'");
	}

	if (triggerPrice == 0 && (type == OrderType::stop
			|| type == OrderType::stoplimit
			|| type == OrderType::oco_limitstop
		)) {
		throw OrderErrorException(id, OrderErrorException::invalidOrMissingStopPrice, "Invalid or missing 'stopPrice'");
	}



	domPriority = data.domPriority;
	queuePriority = data.queuePriority;
	queuePos = pos_counter++;



}


POrder quark::Order::changeState(State newState) const {

	if (newState == state) return const_cast<Order *>(this);
	Order *x = new Order(*this);
	x->state = newState;
	x->queuePos = pos_counter++;
	active = false;
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
	active = false;
	return x;
}

bool Order::willBeFilled(std::size_t price, std::size_t size) const {
	if (budget == 0) return size == this->size;
	else return (size == this->size)
			|| (budget - price*size < price);
}


POrder Order::changeType(OrderType::Type newType) const {
	Order *x = new Order(*this);
	x->type= newType;
	x->queuePos = pos_counter++;
	active = false;
	return x;
}

bool quark::Order::isSimpleUpdate(const Order& other) const {
	return id == other.id
			&& limitPrice == other.limitPrice
			&& type == other.type
			&& dir == other.dir
			&& user == other.user;

}

POrder Order::doSimpleUpdate(const Order& other) const {
	Order *x = new Order(*this);	
	x->id = id;
	x->triggerPrice = other.triggerPrice;
	x->size = other.size;
	x->domPriority = other.domPriority;
	x->queuePriority = other.queuePriority;
	x->budget = other.budget;
	x->data = other.data;
	x->trailingDistance = other.trailingDistance;	
	active = false;
	return x;
}

std::size_t Order::getSizeAtPrice(std::size_t price) const {
	if (budget == 0) return size;
	std::size_t maxsize = budget/price;
	return std::min(maxsize,size);
}

std::intptr_t Order::calcTrailingMove(std::size_t refPrice, std::size_t newPrice, bool rev) const {
	std::uintptr_t calcPos;
	OrderDir::Type d = dir;
	if (rev) {
		switch (d) {
		case OrderDir::buy: d = OrderDir::sell;break;
		case OrderDir::sell: d = OrderDir::buy;break;
		}
	}
	switch (d) {
	case OrderDir::buy:
		calcPos = newPrice+trailingDistance;
		if (calcPos < refPrice) {
			return calcPos - refPrice;
		}
		break;
	case OrderDir::sell:
		if (trailingDistance > newPrice+1) calcPos = 1;
		else calcPos = newPrice-trailingDistance;
		if (calcPos > refPrice) {
			return calcPos - refPrice;
		}
		break;
	}
	return 0;
}

POrder Order::updateTrailing(std::size_t newPrice) const {
	POrder newOrder;
	std::intptr_t diff;
	switch (type) {
	case OrderType::stop:
	case OrderType::oco_limitstop:
		newOrder = new Order(*this);
		active = false;
		newOrder->triggerPrice += calcTrailingMove(triggerPrice, newPrice, false);
		break;
	case OrderType::stoplimit:
		newOrder = new Order(*this);
		active = false;
		diff = calcTrailingMove(triggerPrice, newPrice, false);
		newOrder->triggerPrice+= diff;
		newOrder->limitPrice+=diff;
	case OrderType::limit:
		newOrder = new Order(*this);
		active = false;
		diff = calcTrailingMove(triggerPrice, newPrice, true);
		newOrder->limitPrice+=diff;
		break;
	default:
		newOrder = const_cast<Order *>(this);
	};
	return newOrder;

}


json::NamedEnum<Order::State> orderStateStr(
		{
	{Order::prepared,"prepared"},
	{Order::marketQueue,"marketQueue"},
	{Order::orderbook,"orderbook"},
	{Order::stopQueue,"stopQueue"},
	{Order::oco,"oco"}
		}
);


json::Value Order::toJson() const {
	return json::Object("id",id)
			("user",user)
			("data",data)
			("size",size)
			("budget",budget)
			("limitPrice",limitPrice)
			("triggerPrice",triggerPrice)
			("trailingDistance",trailingDistance)
			("domPriority",domPriority)
			("queuePriority",queuePriority)
			("queuePos",queuePos)
			("dir",OrderDir::str[dir])
			("type", OrderType::str[type])
			("state", orderStateStr[state]);
}


} /* namespace quark */

