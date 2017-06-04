#pragma once

#include <mutex>

#include "logfile.h"

namespace quark {

class CouchDBLogProvider: public ILogProviderFactory {
public:

	void sendLog(ILogProvider::LogType type, json::Value message);
	virtual PLogProvider createLogProvider();

protected:
	std::mutex lock;
};

}

