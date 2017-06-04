#include <imtjson/string.h>
#include "logfile.h"

namespace quark {

class StdErrLogProvider: public ILogProvider {
public:
	virtual void sendLog(LogType type, json::Value message) {

		time_t now;
		time(&now);
		struct tm t;
		gmtime_r(&now,&t);

		char datetext[100];

		strftime(datetext, sizeof(datetext)-1, "%Y-%m-%d %H:%M:%S", &t);

		const char *stype;

		switch(type) {
		case error: stype = "Error";break;
		case warning: stype = "Warn.";break;
		case info: stype = "Info ";break;
		case debug: stype = "Debug";break;
		default: stype = "Unkn.";break;
		}

		std::cerr << datetext << " " << stype << " " << message.toString() << std::endl;
	}


};

class StdErrLogProviderFactory: public ILogProviderFactory {
public:
	virtual PLogProvider createLogProvider() {
		return new StdErrLogProvider();
	}
};

static StdErrLogProviderFactory stdErrLogProvider;
thread_local PLogProvider curLogProvider;
static ILogProviderFactory *curlogProviderFactory = &stdErrLogProvider;


ILogProviderFactory *setLogProvider(ILogProviderFactory* provider) {
	ILogProviderFactory *x = curlogProviderFactory;
	curlogProviderFactory = provider;
	return x;
}

void logError(json::Value v) {
	ILogProvider::globSendLog(ILogProvider::error, v);
}

void logWarn(json::Value v) {
	ILogProvider::globSendLog(ILogProvider::warning, v);
}

void logInfo(json::Value v) {
	ILogProvider::globSendLog(ILogProvider::info, v);
}

void logDebug(json::Value v) {
	ILogProvider::globSendLog(ILogProvider::debug, v);
}

void ILogProvider::globSendLog(LogType type, json::Value message) {
	if (curLogProvider == nullptr) curLogProvider = curlogProviderFactory->createLogProvider();
	curLogProvider->sendLog(type,message);
}


}

