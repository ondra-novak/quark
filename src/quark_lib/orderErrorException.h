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

	static const int orderHasNoID = 1000;
	static const int unknownDirection = 1001;
	static const int orderTypeNotSupported = 1002;
	static const int invalidOrMissingSize = 1003;
	static const int invalidOrMissingLimitPrice = 1004;
	static const int invalidOrMissingStopPrice = 1005;
	static const int invalidOrMissingTrailingDistance = 1006;
	static const int orderConflict = 1009;
	static const int orderNotFound = 1010;
	static const int orderPostLimitConflict = 1100;
	static const int orderFOKFailed = 1101;
	static const int orderIOCCanceled = 1102;
	static const int emptyOrderbook = 1103;
	static const int insufficientFunds = 1104;
	static const int orderSelfTradingCanceled = 1105;
	static const int internalError = 1500;


protected:

	json::Value orderId;
	int code;
	std::string message;

};

} /* namespace quark */


