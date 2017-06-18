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
				bool fullBuy,
				bool fullSell,
				std::size_t size,
				std::size_t price,
				OrderDir::Type dir):AbstractTradeResult(trTrade)
		,buyOrder(buyOrder)
		,sellOrder(sellOrder)
		,fullBuy(fullBuy)
		,fullSell(fullSell)
		,size(size)
		,price(price)
		,dir(dir) {}

	const POrder& getBuyOrder() const {
		return buyOrder;
	}

	OrderDir::Type getDir() const {
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

	bool isFullBuy() const {
		return fullBuy;
	}

	bool isFullSell() const {
		return fullSell;
	}

protected:
	POrder buyOrder;
	POrder sellOrder;
	bool fullBuy;
	bool fullSell;
	std::size_t size;
	std::size_t price;
	OrderDir::Type dir;
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


class TradeResultOrderCancel: public AbstractTradeResult {
public:
	TradeResultOrderCancel(POrder order, std::size_t code)
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


