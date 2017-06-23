#pragma once

#include <memory>
#include <thread>
#include <couchit/changes.h>
#include <couchit/document.h>



#include "imoneysrvclient.h"

namespace quark {

class TradeReplay {
public:
	TradeReplay();
	~TradeReplay();


	void addSvc(PMoneySrvClient svc);


	void start(couchit::CouchDB *tradeDb, couchit::CouchDB *orderDb);
	void stop();


protected:

	bool worker(const couchit::ChangedDoc &chdoc);

	std::vector<PMoneySrvClient> svclist;

	std::thread thr;
	std::unique_ptr<couchit::ChangesFeed> chfeed;
	Value lastTrade;

	couchit::CouchDB *orderDb = nullptr, *tradeDb = nullptr;


	void extractTrade(const couchit::Document &tradeDoc, IMoneySrvClient::TradeData &tdata);
	void extractBalanceChange(const couchit::Document &tradeDoc, StrViewA orderKey, IMoneySrvClient::BalanceChange &tdata, OrderDir::Type dir);
	void resync(PMoneySrvClient target, const Value fromTrade, const Value toTrade);

	Value findTradeCounter(Value trade);

};

} /* namespace quark */
