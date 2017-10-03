/*
 * quarkApp.h
 *
 *  Created on: Jun 3, 2017
 *      Author: ondra
 */

#pragma once
#include <future>
#include <couchit/couchDB.h>
#include <couchit/localView.h>
#include <imtjson/value.h>
#include <unordered_map>
#include <unordered_set>

#include "../common/dispatcher.h"
#include "../common/watchdog.h"


#include "../quark_lib/core.h"
#include "orderBudget.h"

#include "marketConfig.h"
#include "moneyService.h"
#include "tradeHelpers.h"
#include <random>
#include <imtjson/rpc.h>

namespace quark {

using namespace couchit;
using namespace json;


class OrderErrorException;

class QuarkApp: public RefCntObj, public IMoneySrvClientSupport {
public:
	QuarkApp();

	void processOrder(Value cmd);
	void receiveMarketConfig();
	void initialReceiveMarketConfig();
	void applyMarketConfig(Value doc);

	bool start(Value cfg, String signature);
	bool initDb(Value cfg, String signature);


	void exitApp();



protected:
	void monitorQueue(std::promise<Action> &exitFnStore);

	String signature;
	PCouchDB ordersDb;
	PCouchDB tradesDb;
	PCouchDB positionsDb;
	PMoneyService moneyService;
	PMoneySrvClient moneySrvClient;


	PMarketConfig marketCfg;
	static const StrViewA marketConfigDocName;
	static const StrViewA errorDocName;
	static const StrViewA controlDocName;
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
	Dispatcher dispatcher;
	std::thread changesReader;
	std::default_random_engine rnd;


	///each transaction must have unique id
	std::size_t transactionCounter = 0;
	///each trade has unique index
	std::size_t tradeCounter = 0;
	///id of last known trade
	Value lastTradeId = null;

	double lastPrice;

	bool exitCode;





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
	void freeBudget(const Document& order);
	bool blockOnError(ChangesFeed &chfeed);

	json::RpcServer controlServer;
	Watchdog watchdog;

	void execControlOrder(Value cmd);

	void controlStop(RpcRequest req);
	void controlDumpState(RpcRequest req);


	void updateConfig();
	bool updateConfigFromUrl(String url, Value lastModified, Value etag);

	virtual void resync(ITradeStream &target, const Value fromTrade, const Value toTrade);
	virtual bool cancelAllOrders(const json::Array &users);
	virtual Dispatcher &getDispatcher();
};

typedef RefCntPtr<QuarkApp> PQuarkApp;

} /* namespace quark */


