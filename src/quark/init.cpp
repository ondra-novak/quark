#include "init.h"
#include "designdocs/orders.design.h"
#include "designdocs/users.design.h"
#include "designdocs/orderbook.design.h"
#include "designdocs/trades.design.h"
namespace quark {

using namespace couchit;


void initOrdersDB(CouchDB &db) {

	db.putDesignDocument(designdoc_orders,designdoc_orders_length);
	db.putDesignDocument(designdoc_users,designdoc_users_length);
	db.putDesignDocument(designdoc_orderbook,designdoc_orderbook_length);
}




void initTradesDB(couchit::CouchDB& db) {
	db.putDesignDocument(designdoc_trades,designdoc_trades_length);
}

void initPositionsDB(couchit::CouchDB& db) {
}

}
