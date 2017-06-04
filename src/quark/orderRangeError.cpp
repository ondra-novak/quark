/*
 * orderRangeError.cpp
 *
 *  Created on: Jun 4, 2017
 *      Author: ondra
 */

#include "orderRangeError.h"

namespace quark {

OrderRangeError::OrderRangeError(json::Value orderId, int code, double rangeValue)
	:OrderErrorException(orderId,code, "Order param out of range"), rangeValue(rangeValue)
{
	// TODO Auto-generated constructor stub

}

} /* namespace quark */
