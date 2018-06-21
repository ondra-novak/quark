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

#include "shared/dispatcher.h"
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
using ondra_shared::Dispatcher;
using ondra_shared::MsgQueue;


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

	void syncTS(Value cfg, String signature);


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
	typedef std::unordered_map<Value, double> FeeMap;



	OrderBudget calculateBudget(const Document &order);
	void runTransaction(const TxItem &txitm);

	typedef std::vector<Document> TradeList;

	void receiveResults(const ITradeResult &res, OrdersToUpdate &o2u, TradeList &trades);
	void rejectOrderBudget(Document order, bool update);
	void rejectOrder(Document order, const OrderErrorException &e, bool update);

	void initMoneyService();

private:
	POrder docOrder2POrder(const Document& order, double marketBuyBudget);


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
	MsgQueue<bool> schedulerIntr;
	std::thread changesReader;
	std::thread scheduler;
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
	Array queryKeysArray;
	FeeMap feeMap;		//< map which stores data to calculate fees
	std::mutex feeMapLock; //< lock of the feeMap



	bool runOrder(Document order, bool update);
	void runOrder2(Document order, bool update, double marketBuyBudget);
	void freeBudget(const Document& order);
	bool blockOnError(ChangesFeed &chfeed);

	json::RpcServer controlServer;
	Watchdog watchdog;

	void execControlOrder(Value cmd);

	void controlStop(RpcRequest req);
	void controlDumpState(RpcRequest req);
	void controlDumpBlocked(RpcRequest req);


	void updateConfig();
	bool updateConfigFromUrl(String url, Value lastModified, Value etag);

	void createNextOrder(Value originOrder, Value nextOrder);

	void schedulerWorker();
	void schedulerCycle(time_t now);

	virtual void resync(ITradeStream &target, const Value fromTrade, const Value toTrade);
	virtual bool cancelAllOrders(const json::Array &users);
	virtual Dispatcher &getDispatcher();

	virtual void rememberFee(const Value &user, double fee) ;
	double calcFee(const Value &user, double val);

};

typedef RefCntPtr<QuarkApp> PQuarkApp;

} /* namespace quark */


