
#include "core.h"

#include "orderErrorException.h"

namespace quark {


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
			if (o.second->getDir() == OrderDir::buy) orderbook_bid.insert(o.second);
			else orderbook_ask.insert(o.second);
			break;
		case Order::stopQueue:
			if (o.second->getDir() == OrderDir::buy) stop_above.insert(o.second);
			else stop_below.insert(o.second);
			break;
		case Order::marketQueue:
			market.insert(o.second);
			break;
		case Order::oco:
			if (o.second->getDir() == OrderDir::buy) {
				orderbook_bid.insert(o.second);
				stop_above.insert(o.second);
			} else {
				orderbook_ask.insert(o.second);
				stop_below.insert(o.second);
			}
			break;

		}

		if (o.second->isTrailing()) {
			trailings.push_back(o.first);
		}

	}
}

void CurrentState::resetCurrentState() {
	for (auto && o : changes->getChanges()) {

		if (o.second.oldOrder == nullptr)
			orders.erase(o.first);
		else
			orders[o.first] = o.second.oldOrder;
	}
}

void CurrentState::rollbackOneStep() {
	resetCurrentState();
	changes = changes->getPrevState();
}


void CurrentState::reset() {

	rollbackOneStep();
	rebuildQueues();
}


POrder CurrentState::updateOrder(const OrderId &orderId, const POrder &newOrder) {

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
	return newOrder;

}

std::size_t CurrentState::calcBudgetForMarketOrder(OrderDir::Type direction,
		std::size_t size) const {
}


CurrentState::OrderQueue& CurrentState::selectOpositeOrderbook(OrderDir::Type direction) {
	switch (direction) {
	case OrderDir::buy: return orderbook_ask;
	case OrderDir::sell: return orderbook_bid;
	default: throw std::runtime_error("Corrupted order");
	}
}

CurrentState::OrderQueue& CurrentState::selectFriendlyOrderbook(OrderDir::Type direction) {
	switch (direction) {
	case OrderDir::buy: return orderbook_bid;
	case OrderDir::sell: return orderbook_ask;
	default: throw std::runtime_error("Corrupted order");
	}
}

CurrentState::OrderQueue& CurrentState::selectStopQueue(OrderDir::Type direction) {
	switch (direction) {
	case OrderDir::buy: return stop_above;
	case OrderDir::sell: return stop_below;
	default: throw std::runtime_error("Corrupted order");
	}
}

const CurrentState::OrderQueue& CurrentState::selectOpositeOrderbook(
		OrderDir::Type direction) const {
	switch (direction) {
	case OrderDir::buy: return orderbook_ask;
	case OrderDir::sell: return orderbook_bid;
	default: throw std::runtime_error("Corrupted order");
	}
}

const CurrentState::OrderQueue& CurrentState::selectFriendlyOrderbook(
		OrderDir::Type direction) const {
	switch (direction) {
	case OrderDir::buy: return orderbook_bid;
	case OrderDir::sell: return orderbook_ask;
	default: throw std::runtime_error("Corrupted order");
	}
}

const CurrentState::OrderQueue& CurrentState::selectStopQueue(OrderDir::Type direction) const {
	switch (direction) {
	case OrderDir::buy: return stop_above;
	case OrderDir::sell: return stop_below;
	default: throw std::runtime_error("Corrupted order");
	}
}

bool CurrentState::isKnownOrder(const OrderId& orderId) const {
	return orders.find(orderId) != orders.end();
}

bool CurrentState::checkMaxSpreadCond() const {
	auto ask = orderbook_ask.begin();
	auto bid = orderbook_bid.begin();
	//in case that part of orderbook is empty, this rule cannot be checked, we need to go on
	if (ask == orderbook_ask.end() || bid == orderbook_bid.end()) return true;
	std::intptr_t spread = (*ask)->getLimitPrice() - (*bid)->getLimitPrice();
	std::intptr_t center = ((*ask)->getLimitPrice() + (*bid)->getLimitPrice()) / 2;
	std::intptr_t spreadpct = (spread*10000)/center;
	return (spreadpct < maxSpread100Pct);



}

void CurrentState::startTransaction(const json::Value& txid) {
	changes = new EngineState(txid, changes);
	clearHistory();
}

void CurrentState::matching(json::Value txid, const Transaction& tx, Output output) {

	startTransaction(txid);
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

			//execute orders from stopped marked queue
			while (!market.empty()) {
				POrder order = *market.begin();
				OrderQueue &orderbook = order->getDir() == OrderDir::buy?orderbook_ask:orderbook_bid;
				if (!pairInQueue(orderbook ,order,output)) {
					break;
				} else {
					market.erase(market.begin());
				}

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
		throw;
	}


}

CurrentState::OrderQueue &CurrentState::getQueueByState(const POrder &order) {
	switch (order->getState())
	{
	case Order::orderbook:
		if (order->getDir() == OrderDir::buy) return orderbook_bid;
		else if (order->getDir() == OrderDir::sell) return orderbook_ask;
		else throw std::runtime_error("corrupted order direction");
	case Order::stopQueue:
		if (order->getDir() == OrderDir::buy) return stop_above;
		else if (order->getDir() == OrderDir::sell) return stop_below;
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
	q.insert(updateOrder(newOrder->getId(), newOrder));
}

void CurrentState::matchNewOrder(POrder order, Output out) {


	curQueue.push(order);
	while (!curQueue.empty()) {
		POrder o = curQueue.top();

		OrderQueue &orderbook = order->getDir() == OrderDir::buy?orderbook_ask:orderbook_bid;
		OrderQueue &insorderbook = order->getDir() == OrderDir::buy?orderbook_bid:orderbook_ask;
		OrderQueue &stopQueue = order->getDir() == OrderDir::buy?stop_above:stop_below;
		curQueue.pop();
		switch (o->getType()) {
		case OrderType::oco_limitstop:
			if (!pairInQueue(orderbook, o, out)) {
				POrder newOrder = updateOrder(o->changeState(Order::oco));
				insorderbook.insert(newOrder);
				stopQueue.insert(newOrder);
			}
			break;
		case OrderType::limit:
			if (!pairInQueue(orderbook, o, out)) {
				POrder newOrder = updateOrder(o->changeState(Order::orderbook));
				insorderbook.insert(newOrder);
			}
			break;
		case OrderType::maker:
			if (willOrderPairNoSpreadCheck(orderbook, o)) {
				cancelOrder(o);
				out(TradeResultOrderCancel(o,OrderErrorException::orderPostLimitConflict));
			} else {
				POrder newOrder = updateOrder(o->changeState(Order::orderbook)->changeType(OrderType::limit));
				insorderbook.insert(newOrder);
				out(TradeResultOrderTrigger(newOrder));
			}
			break;
		case OrderType::stop:
		case OrderType::stoplimit:
			stopQueue.insert(
					updateOrder(o->changeState(Order::stopQueue)));
			break;
		case OrderType::fok:
			if (!pairInQueue(orderbook, o, out)) {
				throw OrderErrorException(o->getId(), OrderErrorException::orderFOKFailed, "FOK failed");
			}
			break;
		case OrderType::market:
			if (!pairInQueue(orderbook, o, out)) {
				market.insert(updateOrder(o->changeState(Order::marketQueue)));
			}
			break;
		case OrderType::ioc:
			if (!pairInQueue(orderbook, o, out)) {
				cancelOrder(o);
				out(TradeResultOrderCancel(o, OrderErrorException::orderIOCCanceled));
			}
			break;

		}
	}



}


bool CurrentState::willOrderPairNoSpreadCheck(OrderQueue& queue, const POrder& order, OrderQueue::iterator *outIter) {
	//pick begin of orderbook
	auto b = queue.begin();

	return willOrderPairNoSpreadCheck(queue,b,order,outIter);
}

bool CurrentState::willOrderPairNoSpreadCheck(OrderQueue& queue, const OrderQueue::iterator &b, const POrder& order, OrderQueue::iterator *outIter) {
	//if there is no order, exit and prevent pair
	if (b == queue.end()) return false;
	//store iterator
	if (outIter) *outIter = b;
	//market order can be executed immediately
	if (order->getType() == OrderType::market) return true;
	//otherwise, check, whether order cross other other in the orderbook queue
	//first part means that order will executed, if there is order before the current order in the orderbook
	//however, we still can perform execution in case that order is after the current order but at the same price
	return queue.inOrder(*b, order) || (*b)->getLimitPrice() == order->getLimitPrice();
}

bool CurrentState::willOrderPair(OrderQueue& queue, const POrder& order, OrderQueue::iterator *outIter) {

	if (maxSpread100Pct) {
		//get ask pointer
		auto ask = orderbook_ask.begin();
		//get bid pointer
		auto bid = orderbook_bid.begin();

		//both pointers are defined
		if (ask != orderbook_ask.end() && bid != orderbook_bid.end()) {
			//calculate absolute spread
			std::intptr_t spread = (*ask)->getLimitPrice() - (*bid)->getLimitPrice();
			//calculate center price
			std::intptr_t center = ((*ask)->getLimitPrice() + (*bid)->getLimitPrice()) / 2;
			//calculate spread in percent
			std::intptr_t spreadpct = (spread*10000)/center;
			//compare with required spread
			if (spreadpct > maxSpread100Pct) {
				//prevent execution when spread test did not pass
				return false;
			}
			//prepare space for iterator
			OrderQueue::iterator b;
			//depend on required queue pick correct iterator
			if (&orderbook_ask == &queue) {

				return willOrderPairNoSpreadCheck(queue,ask,order,outIter);

			}else if (&orderbook_bid == &queue) {

				return willOrderPairNoSpreadCheck(queue,bid,order,outIter);

			} else {
				//this should not happen, however run fallback function
				return willOrderPairNoSpreadCheck(queue,order,outIter);
			}
		}
	}

	return willOrderPairNoSpreadCheck(queue,order,outIter);
}


bool CurrentState::pairInQueue(OrderQueue &queue, const  POrder &order, Output out) {
	OrderQueue::iterator b;
	if (willOrderPair(queue, order, &b)) {

		POrder maker = *b;
		POrder taker = order;

		//determine trading parameters
		//price
		std::size_t curPrice = maker->getLimitPrice();
		//matching size
		std::size_t commonSize = std::min(maker->getSizeAtPrice(curPrice), taker->getSizeAtPrice(curPrice));

		//we need pair orders of different users
		if (maker->getUser() != taker->getUser()) {


			//update maker command
			POrder newMaker = maker->updateAfterTrade(curPrice, commonSize);
			//update taker command
			POrder newTaker = taker->updateAfterTrade(curPrice, commonSize);

			POrder buy, sell;
			bool fullBuy, fullSell;
			if (taker->getDir() == OrderDir::buy) {
				buy = taker;
				sell = maker;
				fullBuy = newTaker == nullptr;
				fullSell = newMaker == nullptr;
			} else {
				buy = maker;
				sell = taker;
				fullBuy = newMaker == nullptr;
				fullSell = newTaker == nullptr;

			}

			//report trade (exception can be thrown here)
			out(TradeResultTrade(buy, sell, fullBuy, fullSell,
								 commonSize,
								 maker->getLimitPrice(),
								 taker->getDir()));

			//erase this order from the orderbook
			queue.erase(b);

			//update maker order
			updateOrder(maker->getId(), newMaker);
			//note that newMaker can be null (fully executed), only if non-null is put to the orderbook
			if (newMaker != nullptr) queue.insert(newMaker);

			//update taker order
			updateOrder(taker->getId(), newTaker);
			//only non-null orders are put back to the market queue
			if (newTaker != nullptr) curQueue.push(newTaker);

			//run stop triggers
			runTriggers(stop_above,curPrice, std::greater<std::size_t>(),out);
			runTriggers(stop_below,curPrice, std::less<std::size_t>(),out);


			//pairing successful
			return true;
		} else {
			//self trading is not allowed
			//larger order cancels smaller
			if (maker->getSize() >= taker->getSize()) {
				updateOrder(taker->getId(), nullptr);
				out(TradeResultOrderCancel(taker, OrderErrorException::orderSelfTradingCanceled));
			}
			if (maker->getSize() <= taker->getSize()) {
				queue.erase(b);
				updateOrder(maker->getId(), nullptr);
				out(TradeResultOrderCancel(maker, OrderErrorException::orderSelfTradingCanceled));
			}
		}
	} else {
		//no pairing is possible at this cointion
		return false;
	}




}

template<typename Cmp>
void CurrentState::runTriggers(OrderQueue& queue, std::size_t price, Cmp cmp, Output out) {

	if (!queue.empty()) {
		auto b = queue.begin();
		while (b != queue.end() && !cmp((*b)->getTriggerPrice(), price)) {

			POrder newOrder;

			POrder f = *b;
			switch (f->getType()) {
			case OrderType::oco_limitstop:
			case OrderType::stop:
				newOrder = f->changeType(OrderType::market);
				updateOrder(newOrder->getId(), newOrder);
				curQueue.push(newOrder);
				out(TradeResultOrderTrigger(newOrder));
				break;
			case OrderType::stoplimit:
				newOrder = f->changeType(OrderType::limit);
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

void CurrentState::clearHistory() {
	json::RefCntPtr<EngineState> st = changes;
	for (unsigned int i = 0; i < maxHistory && st != nullptr; i++) {
		st = st->getPrevState();
	}
	if (st != nullptr) {
		st->erasePrevState();
	}
}



bool CurrentState::rollbackTo(json::Value txid) {
	json::RefCntPtr<EngineState> st = changes;
	while (st != nullptr) {
		if (st->getStateId() == txid) {

			while (changes->getStateId() != txid) {
				rollbackOneStep();
			}
			rebuildQueues();
			return true;

		}
		st = st->getPrevState();
	}
	return false;
}

json::Value CurrentState::getCurrentTx() const {
	if (changes != nullptr) return changes->getStateId();
	else return json::null;
}

std::size_t CurrentState::getLastPrice() const {
	if (changes != nullptr) return changes->getLastPrice();
	else return 0;
}

}
