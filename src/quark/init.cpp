#include "init.h"
#include "designdocs/orders.design.h"
#include "designdocs/users.design.h"
namespace quark {

using namespace couchit;


void initOrdersDB(CouchDB &db) {

	db.putDesignDocument(designdoc_orders,designdoc_orders_length);
	db.putDesignDocument(designdoc_users,designdoc_users_length);
}




void initTradesDB(couchit::CouchDB& db) {
}

void initPositionsDB(couchit::CouchDB& db) {
}

}
