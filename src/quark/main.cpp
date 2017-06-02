/*
 * main.cpp
 *
 *  Created on: Jun 1, 2017
 *      Author: ondra
 */

#include <couchit/couchDB.h>

#include "init.h"

namespace quark {

using namespace couchit;

static std::unique_ptr<CouchDB> ordersDb;


void start() {

	couchit::Config cfg;
	cfg.authInfo.username = "quark";
	cfg.authInfo.password = "quark";
	cfg.baseUrl = "http://localhost:5984/";
	cfg.databaseName = "orders";
	ordersDb = std::unique_ptr<CouchDB>(new CouchDB(cfg));
	initOrdersDB(*ordersDb);


}


}
int main(int , char **) {

	quark::start();


}

