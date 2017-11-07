#include <couchit/document.h>
#include "init.h"
#include "designdocs/index.design.h"
#include "designdocs/trades.design.h"
#include "designdocs/positions.design.h"
#include "../version.h"
namespace quark {

using namespace couchit;

 bool app_version_changed = false;

void initOrdersDB(CouchDB &db) {

	db.putDesignDocument(designdoc_index,designdoc_index_length);
	Document ver;
	ver= db.get("version", CouchDB::flgCreateNew);
	if (ver["version"].getString() != QUARK_VERSION) {
		ver.set("version", QUARK_VERSION);
		db.put(ver);
		app_version_changed = true;
	}
}




void initTradesDB(couchit::CouchDB& db) {
	db.putDesignDocument(designdoc_trades,designdoc_trades_length);
}

void initPositionsDB(couchit::CouchDB& db) {
	db.putDesignDocument(designdoc_positions,designdoc_positions_length);
}

}
