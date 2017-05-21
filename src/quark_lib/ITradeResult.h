#pragma once
#include "order.h"

namespace quark {


enum TradeRestType {
	trTrade,
	trOrderMove,
	trOrderCancel,
	trOrderTrigger,
	trOrderOk
};

class ITradeResult {
public:
	virtual ~ITradeResult() {}
	virtual TradeRestType getType() const = 0;
};



class AbstractTradeResult : public ITradeResult, public json::RefCntObj {
public:
	AbstractTradeResult(TradeRestType t):t(t) {}
	virtual TradeRestType getType() const override {return t;}
protected:
	TradeRestType t;
};



class TradeResultTrade: public AbstractTradeResult {
public:
	TradeResultTrade(
				POrder buyOrder,
				POrder sellOrder,
				std::size_t size,
				std::size_t price,
				Order::Dir dir):AbstractTradeResult(trTrade)
		,buyOrder(buyOrder)
		,sellOrder(sellOrder)
		,size(size)
		,price(price)
		,dir(dir) {}

	const POrder& getBuyOrder() const {
		return buyOrder;
	}

	Order::Dir getDir() const {
		return dir;
	}

	std::size_t getPrice() const {
		return price;
	}

	const POrder& getSellOrder() const {
		return sellOrder;
	}

	std::size_t getSize() const {
		return size;
	}

protected:
	POrder buyOrder;
	POrder sellOrder;
	std::size_t size;
	std::size_t price;
	Order::Dir dir;
};

class TradeResultOderMove: public AbstractTradeResult {
public:
	TradeResultOderMove(POrder order)
		:AbstractTradeResult(trOrderMove),order(order) {}


	const POrder& getOrder() const {
		return order;
	}

protected:
	POrder order;
};


class TradeResultOderCancel: public AbstractTradeResult {
public:
	TradeResultOderCancel(POrder order, std::size_t code)
		:AbstractTradeResult(trOrderCancel),order(order),code(code) {}


	const POrder& getOrder() const {
		return order;
	}

	std::size_t getCode() const {
		return code;
	}

protected:
	POrder order;
	std::size_t code;
};

class TradeResultOrderTrigger: public AbstractTradeResult {
public:
	TradeResultOrderTrigger(POrder order)
		:AbstractTradeResult(trOrderTrigger),order(order) {}


	const POrder& getOrder() const {
		return order;
	}


protected:
	POrder order;

};


class TradeResultOrderOk: public AbstractTradeResult {
public:
	TradeResultOrderOk(POrder order)
		:AbstractTradeResult(trOrderOk),order(order) {}


	const POrder& getOrder() const {
		return order;
	}


protected:
	POrder order;

};

typedef json::RefCntPtr<AbstractTradeResult> PTradeResult;

} /* namespace quark */


