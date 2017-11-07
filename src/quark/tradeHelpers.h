#pragma once
#include <couchit/couchDB.h>
#include <couchit/document.h>
#include <memory>


#include "imoneysrvclient.h"
//#include "../common/dispatcher.h"

namespace quark {

typedef std::shared_ptr<couchit::CouchDB> PCouchDB;

///extract information from trade to TradeData need by MoneyServer
void extractTrade(const couchit::Value &tradeDoc, const couchit::Value &buyorder,
					const couchit::Value &sellorder,IMoneySrvClient::TradeData &tdata);
///extract information from trade to TradeData new version
void extractTrade2(const couchit::Value &tradeDoc, IMoneySrvClient::TradeData &tdata);
///perform resync of money server from fromTrade (excluded) to toTrade (included)
void resync(couchit::CouchDB &orderDB, couchit::CouchDB &tradeDB, ITradeStream &target, Value fromTrade, Value toTrade,const MarketConfig &mcfg);
///Fetch last trade by its internal counter. This is called on beginning of mainloop to sync with the database
Value fetchLastTrade(couchit::CouchDB &tradeDB);


typedef std::function<void()> Action;




}
