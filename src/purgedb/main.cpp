/*
 * main.cpp
 *
 *  Created on: Jul 14, 2017
 *      Author: ondra
 */
#include <unistd.h>
#include <fstream>
#include <stdexcept>

#include <imtjson/value.h>
#include <imtjson/stringview.h>
#include <couchit/couchDB.h>
#include <couchit/document.h>
#include "../common/config.h"

using quark::initCouchDBConfig;



using namespace json;
using namespace couchit;

void createConfig(std::ostream &out);
void createMarket(Value cfg);

void

int main(int argc, char **argv) {

	char **args = argv+1;
	char *name = argv[0];
	int count = argc -1;

	bool autoYes = false;
	bool autoNo = false;
	String outDir (".") ;

	while (count > 0 && args[0][0] == '-') {
		if (args[0][1] == 'y') {autoYes = true;autoNo = false;}
		if (args[0][1] == 'n') {autoNo = true;autoYes = false;}
		if (args[0][1] == 'o' && count > 1) {
			args++;
			count--;
			outDir = args[0];
		}
		args++;
		count--;
	}

	try {
		if (count < 3) {
			std::cerr << "Removes old orders and trades from the database complete. " << std::endl
					 << "This operation can reduce size of the database file." << std::endl
					 << "NOTE: the operation is ireversible. Make sure, that you have " << std::endl
					 << "replicated database before" << std::endl
					 << std::endl
					 << "Usage: " << std::endl
					 << std::endl
					 << name << "[-y][-n][-o dir] <quark's config file>" << "<market-signature>" << "<timestamp>"
					 << std::endl
					 << "-y                      Perform operation without asking" << std::endl
					 << "-n                      Do not purge data, just backup" << std::endl
					 << "-o dir                  Specify output directory for backup" << std::endl
					 << "<quark'config file>     the same config like for the quark daemon" << std::endl
					 << "<market-signature>      name of the market" << std::endl
					 << "<timestamp>             unix timestamp defines end of purgin "
					 << "                          (newer records are untouched) " << std::endl
					 <<  std::endl
					 << "Notes: Only finished orders are removed." <<  std::endl
					 << "       To reduce size of the database after the purge, the database"<< std::endl
					 << "       must be compacted (automaticly or manually)" <<  std::endl;

			return 1;
		}

		String configPath = args[0];
		String market = args[1];
		Value timestamp;
		Value config;
		try {
			timestamp = Value::fromString(args[2]);
			if (timestamp.type() != json::number || (timestamp.flags() & json::numberUnsignedInteger) == 0) {
				std::cerr << "timestamp must be unsigned integer number" << std::endl;
				return 3;
			}
		} catch (...) {
			std::cerr << "Timestamp must be number" << std::endl;
			return 3;
		}

		try {
			std::ifstream fcfg(configPath.c_str(),std::ios::in);
			config = Value::fromStream(fcfg);

		} catch (std::exception &e) {
			std::cerr << "Failed to parse config: " << e.what() << std::endl;
			return 4;
		}

		const char *ordersSuffix = "-orders";
		const char *tradesSuffix = "-trades";

		StrViewA strTradesSfx(tradesSuffix);
		StrViewA strOrderSfx(ordersSuffix);

		CouchDB ordersDB(initCouchDBConfig(config, market, "-orders"));
		CouchDB tradesDB(initCouchDBConfig(config, market, "-trades"));



		std::size_t sz1 = backupDocuments(orderDB, timestamp, out, market, strOrderSfx);
		std::cout << "Backup orders: " << getBackupName(out, market, timestamp, strOrdersSfx) << ", count: " << sz1 << std::endl;

		std::size_t sz2 = backupDocuments(tradesDB, timestamp, out, market, strTradesSfx);
		std::cout << "Backup trades: " << getBackupName(out, market, timestamp, strTradesSfx) << ", count: " << sz2 << std::endl;

		if (!autoNo) {
			if (!autoYes) {

				std::cerr << "Really delete all old records? Operation is irreversible (y/n):";
				std::cin.sync();
				int c = tolower(std::in.get());
				while (c != EOF && c != "y" && c != 'n') {
					std::cerr << "Please enter y(yes) or n(no):";
					std::cin.sync();
					c = tolower(std::in.get());
				}


			}
		}


	} catch (std::exception &e) {
		std::cerr << "Stopped because an error:" << e.what();
		return 2;

	}
}



