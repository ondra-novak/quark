#pragma once

#include <memory>
#include <thread>
#include <couchit/changes.h>
#include <couchit/document.h>



#include "imoneyservice.h"

namespace quark {

class TradeReplay {
public:
	TradeReplay();
	~TradeReplay();


	void addSvc(PMoneyService svc);


	void start(couchit::CouchDB *tradeDb, couchit::CouchDB *orderDb);
	void stop();


protected:

	bool worker(const couchit::ChangedDoc &chdoc);

	std::vector<PMoneyService> svclist;

	std::thread thr;
	std::unique_ptr<couchit::ChangesFeed> chfeed;
	Value lastTrade;

	couchit::CouchDB *orderDb = nullptr, *tradeDb = nullptr;


	void extractTrade(const couchit::Document &tradeDoc, IMoneyService::TradeData &tdata);
	void extractBalanceChange(const couchit::Document &tradeDoc, StrViewA orderKey, IMoneyService::BalanceChange &tdata, OrderDir::Type dir);
	void resync(PMoneyService target, const Value fromTrade, const Value toTrade);

	Value findTradeCounter(Value trade);

};

} /* namespace quark */
