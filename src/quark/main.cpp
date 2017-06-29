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
#include "logfile.h"
#include "marketConfig.h"
#include "mockupmoneyserver.h"
#include "quarkApp.h"

using quark::logInfo;


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
	CouchDB::fldTimestamp = OrderFields::timeModified;

	CouchDBLogProvider logProvider;

json::maxPrecisionDigits=9;



	setLogProvider(&logProvider);

	logInfo("==== START ====");


	if (c != 3) {

		logError("failed to start. Required arguments: <signature>, <path to configuration>");
		return 1;
	}

	const char *signature = args[1];
	const char *cfgpath = args[2];

	couchit::Config cfg;
	Value cfgjson;

	PMoneySrvClient moneyService;

	{
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
		if (!mandatoryField(cfgjson,"moneyService")) return 4;


		cfg.authInfo.username = json::String(cfgjson["username"]);
		cfg.authInfo.password = json::String(cfgjson["password"]);
		cfg.baseUrl = json::String(cfgjson["server"]);
		cfg.databaseName = json::String({cfgjson["dbprefix"].getString(),StrViewA(signature)});

		Value mscfg = cfgjson["moneyService"];
		StrViewA type = mscfg["type"].getString();
		if (type == "mockup") {
			if (!mandatoryField(mscfg,"maxBudget")) return 5;
			if (!mandatoryField(mscfg,"latency")) return 5;
			Value budget = mscfg["maxBudget"];
			OrderBudget b(budget["asset"].getNumber(),budget["currency"].getNumber()
					,budget["marginLong"].getNumber(),budget["marginShort"].getNumber(), 0 ,0);
			std::size_t latency =mscfg["latency"].getUInt();
			moneyService = new MockupMoneyService(b,latency);
		} else if (type == "error") {
			moneyService = new ErrorMoneyService;
		} else {
			logError({"Unknown money service. Following are known",{"mockup","error"}});
			return 6;
		}


	}

	//sleep for two seconds at start-up
	//this is required as the CouchDB can be still in initialization phase
	std::this_thread::sleep_for(std::chrono::seconds(2));

	PQuarkApp app = new QuarkApp;
	std::thread thr([=]{
		try {

			app->start(cfg, signature);
		} catch (std::exception &e) {
			logError({"Quark exited with unhandled exception",e.what()});
			exit(1);
		}
	});
	while (!std::cin.eof()) {
		char c = std::cin.get();;
		if (c == 'x') break;
	}
	app->exitApp();
	thr.join();

}

