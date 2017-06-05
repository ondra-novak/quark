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

	typedef std::unordered_map<Value, Document> OrdersToUpdate;
	typedef std::unordered_map<Value, Value> UsersToUpdate;



	void createOrder(Document order);
	void updateOrder(Document order);
	Document saveOrder(Document order, Object newItems);
	void matchOrder(Document &order);
	BlockedBudget calculateBudget(const Document &order);
	void updateUserBudget(Value user);
	void runTransaction(const TxItem &txitm);

	void receiveResults(const ITradeResult &res, OrdersToUpdate &o2u);
private:
	POrder docOrder2POrder(const Document& order);
	void releaseUserBudget(Value user);
	void allocateUserBudget(Value user, Value v);

	static const StrViewA FIELD_STATUS;
	std::function<void()> exitFn;
	std::size_t transactionCounter = 0;

	OrdersToUpdate o2u_1, o2u_2;
	UsersToUpdate u2u;

};


} /* namespace quark */


