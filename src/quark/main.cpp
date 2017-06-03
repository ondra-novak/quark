/*
 * main.cpp
 *
 *  Created on: Jun 1, 2017
 *      Author: ondra
 */

#include <fstream>
#include <couchit/couchDB.h>
#include <couchit/changes.h>
#include <couchit/document.h>

#include "init.h"
#include "marketConfig.h"

namespace quark {

using namespace couchit;
using namespace json;

static std::unique_ptr<CouchDB> ordersDb;
static std::unique_ptr<CouchDB> tradesDb;
static std::unique_ptr<CouchDB> positionsDb;
static std::unique_ptr<MarketConfig> marketCfg;
StrViewA marketConfigDocName("settings");

void processOrder(Value cmd) {

	if (marketCfg == nullptr) {
		Document doc(cmd);
		doc("status","rejected")
		   ("reason","market is not opened yet")
		   ("finished",true);
		ordersDb->put(doc);
	}

}

void mainloop() {


	ChangesFeed chfeed = ordersDb->createChangesFeed();
	chfeed.setTimeout(-1).includeDocs() >> [](ChangedDoc chdoc) {

		if (chdoc.id == marketConfigDocName) {
			try {
				marketCfg = std::unique_ptr<MarketConfig>(new MarketConfig(chdoc.doc));
			} catch (...) {
				//TODO add log message here
			}
		} else if (chdoc.id.substr(0,8) != "_design/"){
			processOrder(chdoc.doc);
		}
		return true;
	};


}


void receiveMarketConfig() {
	Value doc = ordersDb->get(marketConfigDocName, CouchDB::flgNullIfMissing);
	if (doc != nullptr) {
		marketCfg = std::unique_ptr<MarketConfig>(new MarketConfig(doc));
	}
}



void start(couchit::Config cfg) {





	String dbprefix = cfg.databaseName;
	cfg.databaseName = dbprefix + "orders";
	ordersDb = std::unique_ptr<CouchDB>(new CouchDB(cfg));
	initOrdersDB(*ordersDb);

	cfg.databaseName = dbprefix + "trades";
	tradesDb = std::unique_ptr<CouchDB>(new CouchDB(cfg));
	initTradesDB(*tradesDb);

	cfg.databaseName = dbprefix + "positions";
	positionsDb = std::unique_ptr<CouchDB>(new CouchDB(cfg));
	initPositionsDB(*positionsDb);


	receiveMarketConfig();

	mainloop();


}


}

bool mandatoryField(const json::Value &v, json::StrViewA name) {

	if (v[name].defined()) return true;
	std::cerr << "Config mandatory field is missing: " << name << std::endl;
	return false;

}

int main(int c, char **args) {

	using namespace couchit;
	using namespace json;

	if (c != 2) {
		std::cerr << "failed to start. Required argument - path to configuration" << std::endl;
		return 1;
	}

	const char *cfgpath = args[1];

	couchit::Config cfg;

	{
		Value cfgjson;
		std::ifstream inp(cfgpath, std::ios::in);
		if (!inp) {
			std::cerr << "Failed to open config:" << cfgpath << std::endl;
			return 2;
		}
		try {
			cfgjson = Value::fromStream(inp);
		} catch (std::exception &e) {
			std::cerr << "Config exception: " << e.what() << std::endl;
			return 3;
		}

		if (!mandatoryField(cfgjson,"server")) return 4;
		if (!mandatoryField(cfgjson,"dbprefix")) return 4;


		cfg.authInfo.username = json::String(cfgjson["username"]);
		cfg.authInfo.password = json::String(cfgjson["password"]);
		cfg.baseUrl = json::String(cfgjson["server"]);
		cfg.databaseName = json::String(cfgjson["dbprefix"]);

	}

	try {
		quark::start(cfg);
	} catch (std::exception &e) {
		std::cerr << "Quark exited with unhandled exception: " << e.what() << std::endl;
	}


}

