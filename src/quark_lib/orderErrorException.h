/*
 * orderErrorException.h
 *
 *  Created on: 19. 5. 2017
 *      Author: ondra
 */

#pragma once


#include <imtjson/value.h>
#include <exception>

namespace quark {

class OrderErrorException: public std::exception {
public:
	OrderErrorException(json::Value orderId, int code, const std::string &message);
	~OrderErrorException() throw() {}

	virtual const char *what() const throw() ;

	int getCode() const {
		return code;
	}

	const std::string& getMessage() const {
		return message;
	}

	const json::Value& getOrderId() const {
		return orderId;
	}

protected:

	json::Value orderId;
	int code;
	std::string message;

};

} /* namespace quark */


