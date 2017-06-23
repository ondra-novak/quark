#pragma once
#include <couchit/couchDB.h>
#include <couchit/document.h>

#include "imoneysrvclient.h"

namespace quark {

///extract information from trade to TradeData need by MoneyServer
void extractTrade(const couchit::Value &tradeDoc, IMoneySrvClient::TradeData &tdata);
///extract BalanceChange from the trade and the orders need by MoneyServer
void extractBalanceChange(couchit::CouchDB &orderDB, const couchit::Value &tradeDoc, IMoneySrvClient::BalanceChange &tdata, OrderDir::Type dir);
///perform resync of money server from fromTrade (excluded) to toTrade (included)
void resync(couchit::CouchDB &orderDB, couchit::CouchDB &tradeDB, PMoneySrvClient target, const Value fromTrade, const Value toTrade);
///Fetch last trade by its internal counter. This is called on beginning of mainloop to sync with the database
Value fetchLastTrade(couchit::CouchDB &tradeDB);





}
