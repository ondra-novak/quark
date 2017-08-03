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

#include "../common/msgqueue.h"


#include "../quark_lib/core.h"
#include "orderBudget.h"

#include "marketConfig.h"
#include "moneyService.h"
#include "tradeHelpers.h"

namespace quark {

using namespace couchit;
using namespace json;


class OrderErrorException;

class QuarkApp: public RefCntObj {
public:
	QuarkApp();

	void processOrder(Value cmd);
	void receiveMarketConfig();
	void applyMarketConfig(Value doc);

	void start(Value cfg, String signature);

	void exitApp();

	static String createTradeId(const TradeResultTrade &tr);

protected:
	void mainloop();

	String signature;
	PCouchDB ordersDb;
	PCouchDB tradesDb;
	PCouchDB positionsDb;
	PMoneyService moneyService;
	PMoneySrvClient moneySrvClient;

	PMarketConfig marketCfg;
	static const StrViewA marketConfigDocName;
	CurrentState coreState;

	typedef std::unordered_map<Value, Document> OrdersToUpdate;
	typedef std::unordered_map<Value, Value> UsersToUpdate;
	typedef std::unordered_map<Value, Value> OrderCache;


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

	typedef std::vector<Document> TradeList;

	void receiveResults(const ITradeResult &res, OrdersToUpdate &o2u, TradeList &trades);
	void rejectOrderBudget(Document order, bool update);
	void rejectOrder(Document order, const OrderErrorException &e, bool update);

	void initMoneyService();
	void watchDog();

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



	Action exitFn;
	typedef MsgQueue<Action> Dispatcher;
	Dispatcher dispatcher;
	std::thread changesReader;


	///each transaction must have unique id
	std::size_t transactionCounter = 0;
	///each trade has unique index
	std::size_t tradeCounter = 0;
	///id of last known trade
	Value lastTradeId = null;

	double lastPrice;





	OrdersToUpdate o2u_1, o2u_2, ocache; //prepared maps
	TradeList tradeList; //buffer for trades

	typedef std::unordered_map<Value, std::size_t> OrderRevisions;

	///Stores revisions of updated orders.
	/** This prevents to picking an old data up from the queue. Everytime the order is updated in the
	 * database, its revision is recorded here. Once the order is picked from the queue, if could be
	 * accepted only if its revision is above. Once the revision is above or equal, the record is
	 * removed from the map as well
	 */
	OrderRevisions orderRevisions;



	void recordRevision(Value docId, Value revId);
	void recordRevisions(const Changeset& chset);
	bool checkOrderRev(Value docId, Value revId);
	bool runOrder(Document order, bool update);
	void runOrder2(Document order, bool update);
	void processPendingOrders(Value user);
	void freeBudget(const Document& order);




};

typedef RefCntPtr<QuarkApp> PQuarkApp;

} /* namespace quark */


