#pragma once
#include <imtjson/value.h>
#include <imtjson/string.h>
#include <stdexcept>

namespace quark {

class RuntimeError: public std::exception {
public:

	RuntimeError(json::Value v):v(v) {}

	const char *what() const throw() {
		if (s.empty()) {
			s = v.toString();
		}
		return s.c_str();
	}

	json::Value getData() const {
		return v;
	}

protected:
	json::Value v;
	mutable json::String s;

};

}
