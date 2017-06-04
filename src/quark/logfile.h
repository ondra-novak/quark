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

	static void globSendLog(LogType type, json::Value message);
};

typedef json::RefCntPtr<ILogProvider> PLogProvider;

class ILogProviderFactory {
public:

	virtual PLogProvider createLogProvider() = 0;


};

ILogProviderFactory *setLogProvider(ILogProviderFactory *provider);


void logError(json::Value v);
void logWarn(json::Value v);
void logInfo(json::Value v);
void logDebug(json::Value v);


}
