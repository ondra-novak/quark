/*
 * quarkApp.h
 *
 *  Created on: Jun 3, 2017
 *      Author: ondra
 */

#pragma once
#include <couchit/couchDB.h>
#include <couchit/localView.h>
#include <imtjson/value.h>

#include "../quark_lib/core.h"
#include "blockedBudget.h"

#include "marketConfig.h"

namespace quark {

using namespace couchit;
using namespace json;


class QuarkApp {
public:
	QuarkApp();

	void processOrder(Value cmd);
	void receiveMarketConfig();

	void start(couchit::Config cfg);

	void exitApp();

protected:
	void mainloop();

	std::unique_ptr<CouchDB> ordersDb;
	std::unique_ptr<CouchDB> tradesDb;
	std::unique_ptr<CouchDB> positionsDb;
	PMarketConfig marketCfg;
	static const StrViewA marketConfigDocName;
	CurrentState coreState;



	void createOrder(Document order);
	void updateOrder(Document order);
	Document saveOrder(Document order, Object newItems);
	void matchOrder(Document &order);
	BlockedBudget calculateBudget(const Document &order);
	void updateUserBudget(Value user);
	void runTransaction(const TxItem &txitm);

private:
	POrder docOrder2POrder(const Document& order);

	static const StrViewA FIELD_STATUS;
	std::function<void()> exitFn;
	std::size_t transactionCounter = 0;
};


} /* namespace quark */


