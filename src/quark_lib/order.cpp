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
		throw OrderErrorException(id,1000, "Order has no ID");
	}

	Value jd = data["dir"];
	if (jd.getString() == "buy") {
		dir = buy;
	} else if (jd.getString() == "sell") {
		dir = sell;
	} else {
		throw OrderErrorException(id, 1001, "Uknown dir - The field 'dir' must be either 'buy' or 'sell'");
	}



	StrViewA tp = data["type"].getString();
	if (tp == "market") type = market;
	else if (tp == "limit") type = limit;
	else if (tp == "stop") type = stop;
	else if (tp == "stoplimit") type = stoplimit;
	else if (tp == "postlimit") type = postlimit;
	else if (tp == "ioc") type = ioc;
	else if (tp == "fok") type = fok;
	else
		throw OrderErrorException(id,1002, String({"Order type '", tp, "' is not supported"}).c_str());


	size = data["size"].getUInt();
	limitPrice = data["limitPrice"].getUInt();
	triggerPrice = data["stopPrice"].getUInt();
	if (size == 0)
		throw OrderErrorException(id, 1003, "Invalid or missing 'size'");

	if (limitPrice == 0 && (type == limit
			|| type == stoplimit
			|| type == fok
			|| type == ioc
			|| type == postlimit)) {
		throw OrderErrorException(id, 1004, "Invalid or missing 'limitPrice'");
	}

	if (triggerPrice == 0 && (type == stop
			|| type == stoplimit)) {
		throw OrderErrorException(id, 1004, "Invalid or missing 'stopPrice'");
	}


	domPriority = data["domPriority"].getInt();
	queuePriority = data["queuePriority"].getInt();

}


} /* namespace quark */
