/*
 * couchDBLogProvider.cpp
 *
 *  Created on: Jun 4, 2017
 *      Author: ondra
 */

#include "couchDBLogProvider.h"
#include <imtjson/string.h>

namespace quark {

class LogProvider: public ILogProvider {
public:
	LogProvider(CouchDBLogProvider &owner):owner(owner) {}
	virtual void sendLog(LogType type, json::Value message) {
		owner.sendLog(type,message);
	}
protected:
	CouchDBLogProvider &owner;
};


void CouchDBLogProvider::sendLog(ILogProvider::LogType type, json::Value message) {

	std::lock_guard<std::mutex> _(lock);

	json::StrViewA level;
	switch (type) {
	case ILogProvider::error: level="error";break;
	case ILogProvider::warning: level="info";break;
	case ILogProvider::info: level="info";break;
	default: level="debug";break;
	}

	std::cout<<"[\"log\",";
	json::Value v = message.stringify();
	v.toStream(std::cout);
	std::cout<<",{\"level\":\"" << level << "\"}]" << std::endl;

}

PLogProvider CouchDBLogProvider::createLogProvider() {
	return new LogProvider(*this);
}

} /* namespace quark */
