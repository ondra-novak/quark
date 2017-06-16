/*
 * main.cpp
 *
 *  Created on: Jun 1, 2017
 *      Author: ondra
 */

#include <fstream>
#include <thread>
#include <couchit/couchDB.h>
#include <couchit/changes.h>
#include <couchit/document.h>

#include "rpcApp.h"

using quark::RpcApp;


bool mandatoryField(const json::Value &v, json::StrViewA name) {

	if (v[name].defined()) return true;
	std::cerr << "Config mandatory field is missing: " << name << std::endl;
	return false;

}


int main(int c, char **args) {

	using namespace quark;
	using namespace couchit;
	using namespace json;


	json::maxPrecisionDigits=9;

	if (c != 2) {

		std::cerr << "failed to start. Required argument - path to configuration" << std::endl;
		return 1;
	}

	const char *cfgpath = args[1];

	Value svctable;


	{
		std::ifstream inp(cfgpath, std::ios::in);
		if (!inp) {
			std::cerr << "Failed to open config: " << cfgpath << std::endl;
			return 2;
		}
		try {
			svctable = Value::fromStream(inp);
		} catch (std::exception &e) {
			std::cerr << "Config exception" << e.what() << std::endl;
			return 3;
		}

		for (Value x: svctable) {
			if (!mandatoryField(x,"server")) return 4;
			if (!mandatoryField(x,"dbprefix")) return 4;
		}

	}

	RpcApp rpcApp(svctable);
	rpcApp.run(std::cin, std::cout);

}

