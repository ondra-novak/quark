#pragma once

#include <couchit/couchDB.h>

namespace quark {

void initOrdersDB(couchit::CouchDB &db);
void initTradesDB(couchit::CouchDB &db);
void initPositionsDB(couchit::CouchDB &db);




}
