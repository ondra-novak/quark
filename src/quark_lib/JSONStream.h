/*
 * JSONStream.h
 *
 *  Created on: 19. 5. 2017
 *      Author: ondra
 */

#pragma once
#include <imtjson/parser.h>
#include <imtjson/serializer.h>
#include <iostream>

namespace quark {

class JSONStream {
public:
	JSONStream(std::istream &input, std::ostream &output):in(input),out(output) {}

	json::Value read();
	void write(json::Value v) ;
	bool isEof();

protected:
	std::istream &in;
	std::ostream &out;
};
} /* namespace quark */


