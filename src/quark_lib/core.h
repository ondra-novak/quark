#pragma once

#include "engineState.h"

namespace quark {


class CurrentState {
public:

	CurrentState();

	static std::size_t counter;

	class OrderQueue: public std::set<POrder, OrderCompare> {
	public:
		OrderQueue(OrderCompare cmp):std::set<POrder, OrderCompare>(cmp),cmp(cmp) {}

		bool inOrder(const POrder &a, const POrder &b) const {
			return cmp(a,b);
		}

		void swap(OrderQueue &other) {
			std::set<POrder, OrderCompare>::swap(other);
			std::swap(cmp,other.cmp);
		}

	protected:
		OrderCompare cmp;
	};


	typedef std::vector<OrderId> OrderList;

	typedef std::unordered_map<OrderId, POrder> Orders;

	typedef std::priority_queue<POrder, std::vector<POrder>, OrderCompare> Queue;


	///starts pairing after all orders are loaded
	void startPairing();



	Orders orders;

	///Orderbook ask side
	OrderQueue orderbook_ask;
	///Orderbook bit side
	OrderQueue orderbook_bid;

	///Orders triggered when price is below
	OrderQueue stop_below;
	///Orders triggered when price is above
	OrderQueue stop_above;

	///market execution queue
	/** It contains orders if queue is stopped.
	 */
	OrderQueue market;

	///trailing orders updated everytime the price changes
	OrderList trailings;

	///current execution queue processed during matching
	/** It is always empty after matching is complete */
	Queue curQueue;


	PEngineState changes;

	unsigned int maxHistory = 4;

	///maximum spread in 0.01% (150 = 1.5%)
	std::size_t maxSpread100Pct = 0;

	///maximum spread in 0.01% (150 = 1.5%)
	void setMaxSpread(std::size_t maxSpr) {
		maxSpread100Pct = maxSpr;
	}

	bool checkMaxSpreadCond() const;


	POrder updateOrder(const OrderId &orderId, const POrder &newOrder);
	POrder updateOrder(const POrder &newOrder) {return updateOrder(newOrder->getId(), newOrder);}

	std::size_t calcBudgetForMarketOrder(OrderDir::Type direction, std::size_t size) const;



	bool checkSpread();

	void rebuildQueues();
	void reset();
	void matching(json::Value txid, const Transaction &tx, Output output);


	template<typename Fn>
	void enumOrderQueues(const POrder &order, const Fn &fn);


	void cancelOrder(POrder order);
	void updateOrderInQueues(POrder oldOrder, POrder newOrder);
	void matchNewOrder(POrder order, Output out);


	enum PairResult {
		///no match found
		pairNoMatch,
		///cannot pair because spread is too large
		pairLargeSpread,
		///matched
		pairMatch
	};


	PairResult willOrderPair(OrderQueue &queue, const POrder &order);




	///Core function - performs pairng the order agains to orderbook
	/**
	 *
	 * @param queue orderbook (queue)
	 * @param order order to pair
	 * @param out output function (reports trades and order change)
	 * @retval true pairing successful. Order has been spent, and it is no longer valid
	 * 	(but after partial execution, new version of the order has been created and put to
	 * 	market queue)
	 * @retval false pairing was not executed, arguments are untocuhed
	 *
	 */
	PairResult pairInQueue(OrderQueue &queue,  const POrder &order, Output out);

	void pairInMarketQueue(const POrder &order, Output out);

	template<typename Queue>
	void pairOneStep(Queue &queue, const POrder &maker, const POrder &taker, std::size_t price, Output out);

	template<typename Cmp>
	void runTriggers(OrderQueue &queue, std::size_t price, Cmp cmp, Output out);

	void updateTrailings(std::size_t price, Output out);

	bool rollbackTo(json::Value txid);
	json::Value getCurrentTx() const;
	std::size_t getLastPrice() const;

	OrderQueue &selectOpositeOrderbook(OrderDir::Type direction);
	OrderQueue &selectFriendlyOrderbook(OrderDir::Type direction);
	OrderQueue &selectStopQueue(OrderDir::Type direction);

	const OrderQueue &selectOpositeOrderbook(OrderDir::Type direction) const ;
	const OrderQueue &selectFriendlyOrderbook(OrderDir::Type direction) const ;
	const OrderQueue &selectStopQueue(OrderDir::Type direction) const ;

	bool isKnownOrder(const OrderId &orderId) const;

	json::Value toJson() const;


	///true if market is stopped for initial phase
	bool stopped = true;


	std::size_t centerOfSpread;


protected:
	void resetCurrentState();
	void rollbackOneStep();
	void clearHistory();

private:
	void startTransaction(const json::Value& txid);
};



}
