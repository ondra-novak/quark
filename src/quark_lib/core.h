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
	protected:
		OrderCompare cmp;
	};


	typedef std::vector<OrderId> OrderList;

	typedef std::unordered_map<OrderId, POrder> Orders;

	typedef std::priority_queue<POrder, std::vector<POrder>, OrderCompare> Queue;




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
	OrderQueue market;

	///trailing orders updated everytime the price changes
	OrderList trailings;

	///current execution queue processed during matching
	/** It is always empty after matching is complete */
	Queue curQueue;


	PEngineState changes;

	unsigned int maxHistory = 4;



	void updateOrder(const OrderId &orderId, const POrder &newOrder);




	void rebuildQueues();

	void reset();

	void matching(json::Value txid, const Transaction &tx, Output output);

	OrderQueue &getQueueByState(const POrder &order);


	void cancelOrder(POrder order);
	void updateOrderInQueues(POrder oldOrder, POrder newOrder);
	void matchNewOrder(POrder order, Output out);


	static bool willOrderPair(OrderQueue &queue, const POrder &order);
	bool pairInQueue(OrderQueue &queue, const POrder &order, Output out);

	template<typename Cmp>
	void runTriggers(OrderQueue &queue, std::size_t price, Cmp cmp, Output out);

	void updateTrailings(std::size_t price, Output out);

	bool rollbackTo(json::Value txid);
	json::Value getCurrentTx() const;
	std::size_t getLastPrice() const;

protected:
	void resetCurrentState();
	void rollbackOneStep();
	void clearHistory();

private:
	void startTransaction(const json::Value& txid);
};



}
