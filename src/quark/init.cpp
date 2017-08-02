#include "init.h"
#include "designdocs/index.design.h"
#include "designdocs/trades.design.h"
#include "designdocs/positions.design.h"
namespace quark {

using namespace couchit;


void initOrdersDB(CouchDB &db) {

	db.putDesignDocument(designdoc_index,designdoc_index_length);
}




void initTradesDB(couchit::CouchDB& db) {
	db.putDesignDocument(designdoc_trades,designdoc_trades_length);
}

void initPositionsDB(couchit::CouchDB& db) {
	db.putDesignDocument(designdoc_positions,designdoc_positions_length);
}

}
