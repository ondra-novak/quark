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

	int pos = 1;
	auto getNextArg = [&]() -> const char * {
		if (pos < c) return args[pos++];
		else return nullptr;
	};


	bool quitAfterInit = false;


	if (c < 3) {
		std::cerr << "Usage:" << std::endl << std::endl
			      << args[0] << " [--initdb] <signature> <conf-file>" << std::endl << std::endl
			      << "--initdb          Just initialize database and exit. This is useful to" << std::endl
			      << "                    to bootstrap replicated database without starting" << std::endl
			      << "                    the matching daemon" << std::endl
			      << std::endl
			      << "<signature>       signature of the market (prefix) " << std::endl
			      << "<conf-file>       configuration file" << std::endl;
			      return 1;
	}

	const char *s = getNextArg();
	if (StrViewA(s) == "--initdb") {quitAfterInit = true; s = getNextArg();}
	const char *signature = s;
	s = getNextArg();
	const char *cfgpath = s;



	json::maxPrecisionDigits=9;
	CouchDB::fldTimestamp = OrderFields::timeModified;

	CouchDBLogProvider logProvider;

	json::maxPrecisionDigits=9;



	setLogProvider(&logProvider);



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


	}
	if (!quitAfterInit) {
		logInfo("==== START ====");

		//sleep for two seconds at start-up
		//this is required as the CouchDB can be still in initialization phase
		std::this_thread::sleep_for(std::chrono::seconds(2));
	}

	PQuarkApp app = new QuarkApp;
	if (quitAfterInit) {
		app->initDb(cfgjson, signature);
	} else {
		std::mutex lock;
		bool finish = false;
		std::thread thr([&]{
			try {

				while (app->start(cfgjson, signature) == true) {
					std::lock_guard<std::mutex> _(lock);
					if (finish) break;
					//restart here
					app = new QuarkApp;
				}
			} catch (std::exception &e) {
				logError({"Quark exited with unhandled exception",e.what()});
				exit(1);
			}
		});
		while (!std::cin.eof()) {
			char c = std::cin.get();;
			if (c == 'x') break;
		}
		{
			std::lock_guard<std::mutex> _(lock);
			app->exitApp();
			finish = true;
		}
		thr.join();
	}

}

