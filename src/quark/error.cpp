/*
 * error.cpp
 *
 *  Created on: 23. 6. 2017
 *      Author: ondra
 */

#include "error.h"
namespace quark {


static UnhandledExceptionHandler curHandler;

void setUnhandledExceptionHandler(UnhandledExceptionHandler fn) {
	curHandler = fn;
}

void unhandledException() {
	if (curHandler != nullptr)
		curHandler();
	else
		abort();
}

}

