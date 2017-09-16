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
#include <couchit/couchDB.h>
#include <couchit/document.h>



using namespace json;
using namespace couchit;

void createConfig(std::ostream &out);
void createMarket(Value cfg);


int main(int argc, char **argv) {
	try {
		if (argc < 4) {
			std::cerr << "Removes old orders and trades from the database complete. " << std::end
					 << "This operation can reduce size of the database file." << std::end
					 << "NOTE: the operation is ireversible. Make sure, that you have " << std::endl
					 << "replicated database before" << std::endl
					 << std::endl
					 << "Usage: " << std::endl
					 << std::endl
					 << argv[0] << "<quark's config file>" << "<market-signature>" << "<timestamp>"
					 << std::endl
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

		String config = argv[1];
		String market = argv[2];
		Value timestamp;
		timestamp = Value::fromString(argv[3]);
		if (timestamp.type() != json::number || timestamp.flags & json::)

}



