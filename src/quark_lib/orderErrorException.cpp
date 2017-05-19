/*
 * orderErrorException.cpp
 *
 *  Created on: 19. 5. 2017
 *      Author: ondra
 */

#include "orderErrorException.h"

namespace quark {

OrderErrorException::OrderErrorException(json::Value orderId, int code, const std::string &message)
	:orderId(orderId)
	,code(code)
	,message(message)
{
	// TODO Auto-generated constructor stub

}

const char* OrderErrorException::what() const throw()
{
	return message.c_str();
}

} /* namespace quark */
