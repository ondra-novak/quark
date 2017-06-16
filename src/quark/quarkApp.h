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
#include <unordered_map>
#include <unordered_set>


#include "../quark_lib/core.h"
#include "blockedBudget.h"

#include "marketConfig.h"
#include "moneyService.h"
#include "pendingOrders.h"

namespace quark {

using namespace couchit;
using namespace json;


class OrderErrorException;

class QuarkApp {
public:
	QuarkApp();

	void processOrder(Value cmd);
	void receiveMarketConfig();

	void start(couchit::Config cfg, PMoneyService moneyService);

	void exitApp();

protected:
	void mainloop();

	std::unique_ptr<CouchDB> ordersDb;
	std::unique_ptr<CouchDB> tradesDb;
	std::unique_ptr<CouchDB> positionsDb;
	PMoneyService moneyService;
	PendingOrders<json::Value> pendingOrders;

	PMarketConfig marketCfg;
	static const StrViewA marketConfigDocName;
	CurrentState coreState;

	typedef std::unordered_map<Value, Document> OrdersToUpdate;
	typedef std::unordered_map<Value, Value> UsersToUpdate;



//	void createOrder(Document order);
	void checkUpdate(Document order);
	Document saveOrder(Document order, Object newItems);
	void matchOrder(Document order);
	BlockedBudget calculateBudget(const Document &order);
	void runTransaction(const TxItem &txitm);

	void receiveResults(const ITradeResult &res, OrdersToUpdate &o2u);
	void rejectOrderBudget(Document order, bool update);
	void rejectOrder(const Document &order, const OrderErrorException &e, bool update);

private:
	POrder docOrder2POrder(const Document& order);



	static const StrViewA FIELD_STATUS;
	std::function<void()> exitFn;
	std::size_t transactionCounter = 0;


	OrdersToUpdate o2u_1, o2u_2;

	std::mutex ordLock;
	typedef std::unique_lock<std::mutex> Sync;

	void runOrder(const Document &doc, bool update);
	void runOrder2(const Document &doc, bool update);

};


} /* namespace quark */


