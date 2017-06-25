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
#include "orderBudget.h"

#include "marketConfig.h"
#include "moneyService.h"

namespace quark {

using namespace couchit;
using namespace json;


class OrderErrorException;

class QuarkApp {
public:
	QuarkApp();

	void processOrder(Value cmd);
	void receiveMarketConfig();

	void start(couchit::Config cfg, PMoneySrvClient moneyService);

	void exitApp();

	static String createTradeId(const TradeResultTrade &tr);

protected:
	void mainloop();

	std::unique_ptr<CouchDB> ordersDb;
	std::unique_ptr<CouchDB> tradesDb;
	std::unique_ptr<CouchDB> positionsDb;
	PMoneyService moneyService;
	PMoneySrvClient moneySrvClient;

	PMarketConfig marketCfg;
	static const StrViewA marketConfigDocName;
	CurrentState coreState;

	typedef std::unordered_map<Value, Document> OrdersToUpdate;
	typedef std::unordered_map<Value, Value> UsersToUpdate;


	class PendingOrders {
		typedef std::unordered_map<Value, std::queue<Value> > Map;

		Map orders;
		std::mutex l;
	public:
		void clear();
		bool lock(Value id, const Value &doc);
		Value unlock(Value id);
	};
	PendingOrders pendingOrders;

//	void createOrder(Document order);
	//Document saveOrder(Document order, Object newItems);
	void matchOrder(Document order);
	OrderBudget calculateBudget(const Document &order);
	void runTransaction(const TxItem &txitm);

	void receiveResults(const ITradeResult &res, OrdersToUpdate &o2u, Changeset &trades);
	void rejectOrderBudget(Document order, bool update);
	void rejectOrder(Document order, const OrderErrorException &e, bool update);

private:
	POrder docOrder2POrder(const Document& order);


	/// Processes order (stage 2)
	/**
	 *
	 * @param cmd order to process
	 * @retval true order is executed
	 * @retval false order is pending
	 */

	bool processOrder2(Value cmd);

	static bool isUpdated(const Document &order);
	static bool isCanceled(const Document &order);
	void cancelOrder(Document order);
	bool updateOrder(Document order);

	void syncWithDb();



	std::function<void()> exitFn;


	///each transaction must have unique id
	std::size_t transactionCounter = 0;
	///each trade has unique index
	std::size_t tradeCounter = 0;
	///id of last known trade
	Value lastTradeId = null;

	double lastPrice;



	OrdersToUpdate o2u_1, o2u_2;

	std::mutex ordLock;
	typedef std::unique_lock<std::mutex> Sync;

	bool runOrder(const Document &doc, bool update);
	void runOrder2(Document doc, bool update);
	void processPendingOrders(Value user);
	void freeBudget(const Document& order);
};


} /* namespace quark */


