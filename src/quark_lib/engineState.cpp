/*
 * engineState.cpp
 *
 *  Created on: 19. 5. 2017
 *      Author: ondra
 */

#include <queue>
#include "engineState.h"

#include "orderErrorException.h"


namespace quark {

std::size_t CurrentState::counter = 0;



EngineState::EngineState(Value id, json::RefCntPtr<EngineState> prevState)
	:stateId(id),prevState(prevState)

{
	if (prevState != nullptr) lastPrice = prevState->lastPrice; else lastPrice = 0;

}

bool sortDomPriority(const POrder &a, const POrder &b) {
	if (a->getDomPriority() > b->getDomPriority())
		return true;
	if (a->getDomPriority()==b->getDomPriority()) {
		if (a->getQueuePos() < b->getQueuePos()) {
			return true;
		} else {
			return json::Value::compare(a->getId(),b->getId()) < 0;
		}
	}
}

bool sortMarketPriority(const POrder &a, const POrder &b) {
	if (a->getQueuePriority() > b->getQueuePriority())
		return true;
	if (a->getQueuePriority()==b->getQueuePriority()) {
		if (a->getQueuePos() < b->getQueuePos()) {
			return true;
		} else {
			return json::Value::compare(a->getId(),b->getId()) < 0;
		}
	}
}

bool sortLimitPriceUp(const POrder &a, const POrder &b) {
	if (a->getLimitPrice()<b->getLimitPrice())
		return true;
	if (a->getLimitPrice() == b->getLimitPrice()) {
			return true;
		return sortDomPriority(a,b);
	}
	return false;
}

bool sortLimitPriceDown(const POrder &a, const POrder &b) {
	if (a->getLimitPrice()>b->getLimitPrice())
		return true;
	if (a->getLimitPrice() == b->getLimitPrice()) {
			return true;

		return sortDomPriority(a,b);
	}
	return false;
}
bool sortTrigPriceUp(const POrder &a, const POrder &b) {
	if (a->getTriggerPrice()<b->getTriggerPrice())
		return true;
	if (a->getTriggerPrice() == b->getTriggerPrice()) {
		return sortDomPriority(a,b);
	}
	return false;
}

bool sortTrigPriceDown(const POrder &a, const POrder &b) {
	if (a->getTriggerPrice()>b->getTriggerPrice())
		return true;
	if (a->getTriggerPrice() == b->getTriggerPrice()) {
		return sortDomPriority(a,b);
	}
	return false;
}

bool sortTrailings(const POrder &a, const POrder &b) {
	return json::Value::compare(a->getId(), b->getId()) < 0;
}

CurrentState::CurrentState()
	:orderbook_ask(OrderCompare(&sortLimitPriceUp))
	,orderbook_bid(OrderCompare(&sortLimitPriceDown))
	,stop_above(OrderCompare(&sortTrigPriceUp))
	,stop_below(OrderCompare(&sortTrigPriceDown))
	,market(OrderCompare(&sortMarketPriority))
	,curQueue(OrderCompare([](const POrder &a , const POrder &b) {
		return sortMarketPriority(b,a);}
		)) {}


void CurrentState::rebuildQueues() {
	orderbook_ask.clear();
	orderbook_bid.clear();
	stop_below.clear();
	stop_above.clear();
	trailings.clear();
	market.clear();

	for (auto && o : orders) {

		switch (o.second->getState()) {
		case Order::orderbook:
			if (o.second->getDir() == Order::buy) orderbook_bid.insert(o.second);
			else orderbook_ask.insert(o.second);
			break;
		case Order::stopQueue:
			if (o.second->getDir() == Order::buy) stop_above.insert(o.second);
			else stop_below.insert(o.second);
			break;
		case Order::marketQueue:
			market.insert(o.second);
			break;
		}

		Order::Type t = o.second->getType();
		if (t == Order::trailingLimit || t == Order::trailingStop || t == Order::trailingStopLimit) {
			trailings.push_back(o.first);
		}

	}
}

void CurrentState::reset() {
	for (auto && o : changes->getChanges()) {

		if (o.second.newOrder == nullptr)
			orders.erase(o.first);
		else
			orders[o.first] = o.second.oldOrder;
	}
	changes = changes->getPrevState();
	rebuildQueues();
}


void CurrentState::updateOrder(const OrderId &orderId, const POrder &newOrder) {

	auto &ch = changes->getChanges();
	auto p = ch.find(orderId);
	if (p == ch.end()) {

		POrder &ord = orders[orderId];
		EngineState::OrderUpdate &up = ch[orderId];
		up.newOrder= newOrder;
		up.oldOrder = ord;
		ord = newOrder;
	} else {

		EngineState::OrderUpdate &up = ch[orderId];
		up.newOrder = newOrder;
		POrder &ord = orders[orderId];
		ord = newOrder;

	}

}

void CurrentState::matching(json::Value txid, const Transaction& tx, Output output) {

	changes = new EngineState(txid, changes);
	try {

		std::size_t begPrice = changes->getLastPrice();

		for (auto txi : tx) {

			switch (txi.action) {
			case actionAddOrder: {
				if (orders.find(txi.orderId) != orders.end()) {
					throw OrderErrorException(txi.orderId, OrderErrorException::orderConflict,
							json::String({"Order '", txi.orderId.toString(), "' already exists"}).c_str());
				}
				output(TradeResultOrderOk(txi.order));
				matchNewOrder(txi.order,output);
			}break;
			case actionUpdateOrder: {
				auto it = orders.find(txi.orderId);
				if (it != orders.end()) {
					if (it->second->isSimpleUpdate(*txi.order)) {
						updateOrderInQueues(it->second,it->second->doSimpleUpdate(*txi.order));
					} else {
						cancelOrder(it->second);
						matchNewOrder(txi.order,output);
					}
				} else {
					throw OrderErrorException(txi.orderId, OrderErrorException::orderNotFound,
							json::String({"Order '", txi.orderId.toString(), "' not found"}).c_str());
				}
			}break;
			case actionRemoveOrder: {
				auto it = orders.find(txi.orderId);
				if (it != orders.end()) {
					cancelOrder(it->second);
				} else{
					throw OrderErrorException(txi.orderId, OrderErrorException::orderNotFound,
							json::String({"Order '", txi.orderId.toString(), "' not found"}).c_str());
				}
			}
			break;
			}

		}

		std::size_t endPrice = changes->getLastPrice();

		while (begPrice != endPrice) {
			begPrice = endPrice;
			updateTrailings(endPrice,output);
			endPrice = changes->getLastPrice();
		}


	} catch (...) {
		reset();
	}


}

CurrentState::OrderQueue &CurrentState::getQueueByState(const POrder &order) {
	switch (order->getState())
	{
	case Order::orderbook:
		if (order->getDir() == Order::buy) return orderbook_bid;
		else if (order->getDir() == Order::sell) return orderbook_ask;
		else throw std::runtime_error("corrupted order direction");
	case Order::stopQueue:
		if (order->getDir() == Order::buy) return stop_above;
		else if (order->getDir() == Order::sell) return stop_below;
		else throw std::runtime_error("corrupted order direction");
	case Order::marketQueue:
		return market;
	}
	throw std::runtime_error("corrupted order state");
}

void CurrentState::cancelOrder(POrder order) {

	if (order->getState() != Order::prepared) {
		OrderQueue &q = getQueueByState(order);
		q.erase(order);
	}
	updateOrder(order->getId(), nullptr);
}

void CurrentState::updateOrderInQueues(POrder oldOrder, POrder newOrder) {

	OrderQueue &q = getQueueByState(oldOrder);
	q.erase(oldOrder);
	q.insert(newOrder);
	updateOrder(newOrder->getId(), newOrder);
}

void CurrentState::matchNewOrder(POrder order, Output out) {

	curQueue.push(order);
	while (!curQueue.empty()) {
		POrder o = curQueue.top();

		OrderQueue &orderbook = order->getDir() == Order::buy?orderbook_ask:orderbook_bid;
		OrderQueue &stopQueue = order->getDir() == Order::buy?stop_above:stop_below;
		curQueue.pop();
		switch (o->getType()) {
		case Order::market:
			pairInQueue(orderbook, o, out);
			break;
		case Order::limit:
			if (!pairInQueue(orderbook, o, out)) {
				o = o->changeState(Order::orderbook);
				getQueueByState(o).insert(o);
				updateOrder(o->getId(), o);
			}
			break;
		case Order::postlimit:
			if (willOrderPair(orderbook, o)) {
				cancelOrder(o);
				out(TradeResultOrderCancel(o,OrderErrorException::orderPostLimitConflict));
			} else {
				o = o->changeState(Order::orderbook);
				getQueueByState(o).insert(o);
				updateOrder(o->getId(), o);
			}
			break;
		case Order::stop:
		case Order::stoplimit:
			stopQueue.insert(o->changeState(Order::stopQueue));
			updateOrder(o->getId(), o);
			break;
		case Order::fok:
			if (!pairInQueue(orderbook, o, out)) {
				throw OrderErrorException(o->getId(), OrderErrorException::orderFOKFailed, "FOK failed");
			}
			break;
		case Order::ioc:
			if (!pairInQueue(orderbook, o, out)) {
				cancelOrder(o);
				out(TradeResultOrderCancel(o, OrderErrorException::orderIOCCanceled));
			}
			break;

		}
	}



}

bool CurrentState::willOrderPair(OrderQueue& queue, const POrder& order) {
	if (queue.empty()) return false;
	auto b = queue.begin();
	return queue.inOrder(*b, order);
}


bool CurrentState::pairInQueue(OrderQueue &queue, const POrder &order, Output out) {
	if (queue.empty()) return false;
	auto b = queue.begin();
	if (queue.inOrder(*b, order) || (*b)->getLimitPrice() == order->getLimitPrice()) {

		POrder maker = *b;
		POrder taker = order;

		//determine trading parameters
		//price
		std::size_t curPrice = maker->getLimitPrice();
		//matching size
		std::size_t commonSize = std::min(maker->getSizeAtPrice(curPrice), taker->getSizeAtPrice(curPrice));


		//update maker command
		POrder newMaker = maker->updateAfterTrade(curPrice, commonSize);
		//update taker command
		POrder newTaker = taker->updateAfterTrade(curPrice, commonSize);

		POrder buy, sell;
		bool fullBuy, fullSell;
		if (taker->getDir() == Order::buy) {
			buy = taker;
			sell == maker;
			fullBuy = newTaker == nullptr;
			fullSell = newMaker == nullptr;
		} else {
			buy = maker;
			sell == taker;
			fullBuy = newMaker == nullptr;
			fullSell = newTaker == nullptr;

		}


		out(TradeResultTrade(buy, sell, fullBuy, fullSell,
						     commonSize,
						     maker->getLimitPrice(),
						     taker->getDir()));
		queue.erase(b);

		updateOrder(maker->getId(), newMaker);
		if (newMaker != nullptr) queue.insert(newMaker);

		updateOrder(taker->getId(), newTaker);
		if (newTaker != nullptr) curQueue.push(newTaker);

		runTriggers(stop_above,curPrice, std::greater<std::size_t>(),out);
		runTriggers(stop_below,curPrice, std::less<std::size_t>(),out);


	} else {
		return false;
	}




}

template<typename Cmp>
void CurrentState::runTriggers(OrderQueue& queue, std::size_t price, Cmp cmp, Output out) {

	if (!queue.empty()) {
		auto b = queue.begin();
		while (!cmp((*b)->getTriggerPrice(), price)) {

			POrder newOrder;

			POrder f = *b;
			switch (f->getType()) {
			case Order::stop:
				newOrder = f->changeType(Order::market);
				updateOrder(newOrder->getId(), newOrder);
				curQueue.push(newOrder);
				out(TradeResultOrderTrigger(newOrder));
				break;
			case Order::stoplimit:
				newOrder = f->changeType(Order::limit);
				updateOrder(newOrder->getId(), newOrder);
				curQueue.push(newOrder);
				out(TradeResultOrderTrigger(newOrder));
				break;
			default:
				throw std::runtime_error("Found unsuported order in triggers");
			}

			queue.erase(b);
			b = queue.begin();
		}
	}



}

void CurrentState::updateTrailings(std::size_t price, Output out) {
	auto i = trailings.begin();
	auto e = trailings.end();
	auto p = i;
	while (i != e) {

		auto f = orders.find(*i);
		if (f != orders.end()) {
			POrder o = f->second;
			POrder newO = o->updateTrailing(price);
			if (newO != o) {
				out(TradeResultOderMove(newO));
				cancelOrder(o);
				matchNewOrder(newO,out);
			}
		}
		*p = *i;
		i++;
		p++;
	}
	trailings.resize(p - trailings.begin());
}

} /* namespace quark */
