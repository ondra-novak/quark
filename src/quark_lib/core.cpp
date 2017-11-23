
#include "core.h"

#include "orderErrorException.h"

namespace quark {


bool sortDomPriority(const POrder &a, const POrder &b) {
	if (a->getDomPriority() > b->getDomPriority())
		return true;
	if (a->getDomPriority()==b->getDomPriority()) {
		if (a->getQueuePos() < b->getQueuePos()) {
			return true;
		} else if (a->getQueuePos() == b->getQueuePos()) {
			return json::Value::compare(a->getId(),b->getId()) < 0;
		}
	}
    return false;
}

bool sortMarketPriority(const POrder &a, const POrder &b) {
	if (a->getQueuePriority() > b->getQueuePriority())
		return true;
	if (a->getQueuePriority()==b->getQueuePriority()) {
		if (a->getQueuePos() < b->getQueuePos()) {
			return true;
		} else if (a->getQueuePos() == b->getQueuePos()) {
			return json::Value::compare(a->getId(),b->getId()) < 0;
		}
	}
	return false;
}

bool sortLimitPriceUp(const POrder &a, const POrder &b) {
	if (a->getLimitPrice()<b->getLimitPrice())
		return true;
	if (a->getLimitPrice() == b->getLimitPrice()) {
		return sortDomPriority(a,b);
	}
	return false;
}

bool sortLimitPriceDown(const POrder &a, const POrder &b) {
	if (a->getLimitPrice()>b->getLimitPrice())
		return true;
	if (a->getLimitPrice() == b->getLimitPrice()) {
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
		if (newOrder == nullptr)
			orders.erase(orderId);
		else
			ord = newOrder;
	} else {

		EngineState::OrderUpdate &up = ch[orderId];
		up.newOrder = newOrder;
		if (newOrder == nullptr)
			orders.erase(orderId);
		else
			orders[orderId] = newOrder;

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

void CurrentState::startPairing() {
	stopped = false;
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


		}

		if (!market.empty() && checkSpread() == false) {
			OrderQueue oldMarket((OrderCompare(&sortMarketPriority)));
			market.swap(oldMarket);
			for (POrder item: oldMarket) {
				//execute orders from stopped marked queue
				matchNewOrder(item, output);
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

template<typename Fn>
void CurrentState::enumOrderQueues(const POrder &order, const Fn &fn) {
	switch (order->getState())
	{
	case Order::orderbook:
		if (order->getDir() == OrderDir::buy) fn(orderbook_bid);
		else if (order->getDir() == OrderDir::sell) fn(orderbook_ask);
		else throw std::runtime_error("corrupted order direction");
		break;
	case Order::stopQueue:
		if (order->getDir() == OrderDir::buy) fn(stop_above);
		else if (order->getDir() == OrderDir::sell) fn(stop_below);
		else throw std::runtime_error("corrupted order direction");
		break;
	case Order::marketQueue:
		fn(market);
		break;
	case Order::oco:
		if (order->getDir() == OrderDir::buy) {
			fn(orderbook_bid);
			fn(stop_above);
		}
		else if (order->getDir() == OrderDir::sell) {
			fn(orderbook_ask);
			fn(stop_below);
		}
		else throw std::runtime_error("corrupted order direction");
		break;
	case Order::prepared:
		//nowhere yet
		break;
	default:
		throw std::runtime_error("corrupted order state");
	}

}


void CurrentState::cancelOrder(POrder order) {

	enumOrderQueues(order, [=](OrderQueue &q){
		q.erase(order);
	});
	updateOrder(order->getId(), nullptr);
}

void CurrentState::updateOrderInQueues(POrder oldOrder, POrder newOrder) {

	if (oldOrder->getState() != Order::prepared) {
		enumOrderQueues(oldOrder, [=](OrderQueue &q){
			q.erase(oldOrder);
			q.insert(updateOrder(newOrder->getId(), newOrder));
		});
	}
}

void CurrentState::matchNewOrder(POrder order, Output out) {
	PairResult pres;
	POrder newOrder;


	curQueue.push(order);
	while (!curQueue.empty()) {
		POrder o = curQueue.top();

		OrderQueue &orderbook = o->getDir() == OrderDir::buy?orderbook_ask:orderbook_bid;
		OrderQueue &insorderbook = o->getDir() == OrderDir::buy?orderbook_bid:orderbook_ask;
		OrderQueue &stopQueue = o->getDir() == OrderDir::buy?stop_above:stop_below;
		curQueue.pop();
		switch (o->getType()) {

		case OrderType::oco_limitstop:
			pres = pairInQueue(orderbook, o, out);
			switch (pres) {
				case pairMatch:break;
				case pairNoMatch:
					newOrder = updateOrder(o->changeState(Order::oco));
					stopQueue.insert(newOrder);
					insorderbook.insert(newOrder);
					break;
				case pairLargeSpread:
					pairInMarketQueue(updateOrder(o->changeState(Order::marketQueue)),out);
					break;
			}
			break;

		case OrderType::limit:
			pres = pairInQueue(orderbook, o, out);
			switch (pres) {
				case pairMatch:break;
				case pairNoMatch:
					newOrder = updateOrder(o->changeState(Order::orderbook));
					insorderbook.insert(newOrder);
					break;
				case pairLargeSpread:
					pairInMarketQueue(updateOrder(o->changeState(Order::marketQueue)),out);
					break;
			}
			break;

		case OrderType::maker:
			pres = willOrderPair(orderbook, o);
			if (pres == pairNoMatch) {
				newOrder = updateOrder(o->changeState(Order::orderbook)->changeType(OrderType::limit));
				insorderbook.insert(newOrder);
				out(TradeResultOrderTrigger(newOrder));
			} else {
				cancelOrder(o);
				out(TradeResultOrderCancel(o,OrderErrorException::orderPostLimitConflict));
			}
			break;

		case OrderType::stop:
		case OrderType::stoplimit:
			stopQueue.insert(
					updateOrder(o->changeState(Order::stopQueue)));
			break;

		case OrderType::fok:
			if (pairInQueue(orderbook, o, out) != pairMatch) {
				throw OrderErrorException(o->getId(), OrderErrorException::orderFOKFailed, "FOK failed");
			}
			break;

		case OrderType::market:
			pres = pairInQueue(orderbook, o, out);
			switch (pres) {
			case pairMatch:break;
			case pairLargeSpread:
				pairInMarketQueue(updateOrder(o->changeState(Order::marketQueue)),out);
				break;
			case pairNoMatch:
				cancelOrder(o);
				out(TradeResultOrderCancel(o, OrderErrorException::emptyOrderbook));
				break;
			}
			break;

		case OrderType::ioc:
			pres = pairInQueue(orderbook, o, out);
			switch (pres) {
				case pairMatch:break;
				case pairLargeSpread:
					pairInMarketQueue(updateOrder(o->changeState(Order::marketQueue)),out);
					break;
				case pairNoMatch:
					cancelOrder(o);
					out(TradeResultOrderCancel(o, OrderErrorException::orderIOCCanceled));
					break;
			}
			break;

		}
	}



}

bool CurrentState::checkSpread() {

	//get ask pointer
	auto ask = orderbook_ask.begin();
	//get bid pointer
	auto bid = orderbook_bid.begin();

	//both pointers are defined
	if (ask != orderbook_ask.end() && bid != orderbook_bid.end()) {

		if (maxSpread100Pct) {
			std::intptr_t askval = (*ask)->getLimitPrice();

			std::intptr_t bidval = (*bid)->getLimitPrice();
			//calculate absolute spread
			std::intptr_t spread = askval - bidval;
			//calculate center price
			std::intptr_t center = (askval + bidval)/2;
			//calculate spread in percent
			std::intptr_t spreadpct = (spread*10000)/center;


			centerOfSpread = center;

			//compare with required spread
			return spreadpct > maxSpread100Pct;
		} else {
			return false;
		}
	} else {
		return false;
	}

}

CurrentState::PairResult CurrentState::willOrderPair(OrderQueue& queue, const POrder& order) {

	//pick begin of orderbook
	auto b = queue.begin();

	//if there is no order, exit and prevent pair
	if (b == queue.end()) return pairNoMatch;

	//market order can be executed immediately
	if (order->getType() != OrderType::market) {

		const POrder &a = *b;
		//check for matching order
		if (!queue.inOrder(a, order) &&  a->getLimitPrice() != order->getLimitPrice())

			//nothing found, no match
			return pairNoMatch;
	}

	if (stopped) {
		return pairLargeSpread;
	}

	//so match can be found
	//check spread now
	return checkSpread()?pairLargeSpread:pairMatch;


}

template<typename Q>
void CurrentState::pairOneStep(Q &queue, const POrder &maker, const POrder &taker, std::size_t curPrice, Output out) {
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
							 curPrice,
							 taker->getDir()));

		//erase this order from the orderbook
		queue.erase(maker);

		//update maker order
		updateOrder(maker->getId(), newMaker);
		//note that newMaker can be null (fully executed), only if non-null is put to the orderbook
		if (newMaker != nullptr) queue.insert(newMaker);

		//update taker order
		updateOrder(taker->getId(), newTaker);
		//only non-null orders are put back to the market queue
		if (newTaker != nullptr) curQueue.push(newTaker);

		changes->setLastPrice(curPrice);

		//run stop triggers
		runTriggers(stop_above,curPrice, std::greater<std::size_t>(),out);
		runTriggers(stop_below,curPrice, std::less<std::size_t>(),out);


	} else {
		//self trading is not allowed
		//larger order cancels smaller
		if (maker->getSize() >= taker->getSize()) {
			updateOrder(taker->getId(), nullptr);
			out(TradeResultOrderCancel(taker, OrderErrorException::orderSelfTradingCanceled));
		}
		if (maker->getSize() <= taker->getSize()) {
			queue.erase(maker);
			updateOrder(maker->getId(), nullptr);
			out(TradeResultOrderCancel(maker, OrderErrorException::orderSelfTradingCanceled));
		}
	}

}

class CurrentState::FakeQueue {
public:
	FakeQueue(CurrentState &owner):owner(owner) {}
	void erase(const POrder &o) {
		owner.market.erase(o);
	}
	void insert(const POrder &o) {
		owner.curQueue.push(o);
	}

private:
	CurrentState &owner;
};

void CurrentState::pairInMarketQueue(const POrder& order, Output out) {

		auto iter = market.begin();
		if (iter == market.end() || stopped) {
			market.insert(order);
			out(TradeResultOrderDelayStatus(order));
			return;
		} else {
			const POrder &a = *iter;
			if (a->getDir() == order->getDir()) {
				market.insert(order);
				out(TradeResultOrderDelayStatus(order));
				return;
			}
			else {


				POrder maker = *iter;
				POrder taker = order;

				checkSpread();

				FakeQueue q(*this);


				if (maker->checkCond(centerOfSpread)) {
					pairOneStep(q, maker, taker, centerOfSpread, out);
				} else {
					matchNewOrder(maker, out);
					pairInMarketQueue(order,out);
				}

			}
		}

}


CurrentState::PairResult CurrentState::pairInQueue(OrderQueue &queue, const  POrder &order, Output out) {

	CurrentState::PairResult r = willOrderPair(queue, order);

	if (r != pairMatch) return r;

	OrderQueue::iterator b = queue.begin();

	POrder maker = *b;
	POrder taker = order;

	//determine trading parameters
	//price
	std::size_t curPrice = maker->getLimitPrice();

	pairOneStep(queue, maker, taker, curPrice, out);

	//pairing successful
	return pairMatch;


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
				orderbook_ask.erase(f);
				orderbook_bid.erase(f);
				newOrder = f->changeType(OrderType::market);
				updateOrder(newOrder->getId(), newOrder);
				curQueue.push(newOrder);
				out(TradeResultOrderTrigger(newOrder));
				break;
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

static json::Value dumpQueue(const CurrentState::OrderList &list) {
	json::Array res;
	res.reserve(list.size());
	for (auto x: list) {
		res.push_back(x);
	}
	return res;
}

static json::Value dumpQueue(const CurrentState::OrderQueue &list) {
	json::Array res;
	res.reserve(list.size());
	for (auto x: list) {
		res.push_back(x->toJson());
	}
	return res;

}


static json::Value dumpQueue(const CurrentState::Queue &list) {
	json::Array res;
	res.reserve(list.size());
	CurrentState::Queue cpy = list;
	while (!cpy.empty()) {
		res.push_back(cpy.top()->toJson());
		cpy.pop();
	}
	return res;

}

json::Value quark::CurrentState::toJson() const {
	json::Object out;
	out("orderbook_ask", dumpQueue(orderbook_ask))
		("orderbook_bid",dumpQueue(orderbook_bid))
		("stop_below",dumpQueue(stop_below))
		("stop_above",dumpQueue(stop_above))
		("trailings",dumpQueue(trailings))
		("market",dumpQueue(market));
	return out;
}


}

