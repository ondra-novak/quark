#include "init.h"
#include "designdocs/orders.design.h"
namespace quark {

using namespace couchit;


void initOrdersDB(CouchDB &db) {

	db.putDesignDocument(designdoc_orders,designdoc_orders_length);
}




void initTradesDB(couchit::CouchDB& db) {
}

void initPositionsDB(couchit::CouchDB& db) {
}

}
