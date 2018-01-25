#pragma once

#include <imtjson/refcnt.h>
#include <imtjson/value.h>

namespace quark {

class ILogProvider: public json::RefCntObj {
public:

	enum LogType {

		error,
		warning,
		info,
		debug

	};

	virtual void sendLog(LogType type, json::Value message) = 0;

	virtual ~ILogProvider() {}
	static void globSendLog(LogType type, json::Value message);
};

typedef json::RefCntPtr<ILogProvider> PLogProvider;

class ILogProviderFactory {
public:

	virtual PLogProvider createLogProvider() = 0;
	virtual ~ILogProviderFactory() {}


};

ILogProviderFactory *setLogProvider(ILogProviderFactory *provider);
PLogProvider setLogProvider(PLogProvider p);


void logError(json::Value v);
void logWarn(json::Value v);
void logInfo(json::Value v);
void logDebug(json::Value v);

#ifdef DISABLE_DEBUGLOG
#define LOGDEBUG1(x)
#define LOGDEBUG2(x,y)
#define LOGDEBUG3(x,y,z)
#define LOGDEBUG4(x,y,z,w)
#else
#define LOGDEBUG1(x) logDebug({(x)})
#define LOGDEBUG2(x,y) logDebug({(x),(y)})
#define LOGDEBUG3(x,y,z) logDebug({(x),(y),(z)})
#define LOGDEBUG4(x,y,z,w) logDebug({(x),(y),(z),(w)})
#endif


}
