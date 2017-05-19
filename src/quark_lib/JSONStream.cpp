/*
 * JSONStream.cpp
 *
 *  Created on: 19. 5. 2017
 *      Author: ondra
 */

#include "JSONStream.h"

namespace quark {

json::Value JSONStream::read() {
	return json::Value::fromStream(in);
}

void JSONStream::write(json::Value v) {
	v.toStream(out);
	out << std::endl;
}

bool JSONStream::isEof() {
	int c = in.peek();
	while (c != EOF && isspace(c)) {
		in.get();
		c = in.peek();
	}
	return c == EOF;
}


} /* namespace quark */
