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

#include "couchDBLogProvider.h"

#include "init.h"
#include "marketConfig.h"
#include "quarkApp.h"

quark::QuarkApp quarkApp;

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

	CouchDBLogProvider logProvider;

	setLogProvider(&logProvider);

	if (c != 2) {

		logError("failed to start. Required argument - path to configuration");
		return 1;
	}

	const char *cfgpath = args[1];

	couchit::Config cfg;

	{
		Value cfgjson;
		std::ifstream inp(cfgpath, std::ios::in);
		if (!inp) {
			logError({"Failed to open config",cfgpath});
			return 2;
		}
		try {
			cfgjson = Value::fromStream(inp);
		} catch (std::exception &e) {
			logError({"Config exception",e.what()});
			return 3;
		}

		if (!mandatoryField(cfgjson,"server")) return 4;
		if (!mandatoryField(cfgjson,"dbprefix")) return 4;


		cfg.authInfo.username = json::String(cfgjson["username"]);
		cfg.authInfo.password = json::String(cfgjson["password"]);
		cfg.baseUrl = json::String(cfgjson["server"]);
		cfg.databaseName = json::String(cfgjson["dbprefix"]);

	}

	std::thread thr([=]{
		try {
			quarkApp.start(cfg);
		} catch (std::exception &e) {
			logError({"Quark exited with unhandled exception",e.what()});
		}
		exit(1);
	});
	while (!std::cin.eof()) {
		char c = std::cin.get();;
		if (c == 'x') break;
	}
	quarkApp.exitApp();

}

