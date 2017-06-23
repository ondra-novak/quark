#pragma once


#include <functional>
namespace quark {


void unhandledException();

typedef std::function<void()> UnhandledExceptionHandler;

void setUnhandledExceptionHandler(UnhandledExceptionHandler fn);


}
