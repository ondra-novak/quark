#pragma once
#include "order.h"

namespace quark {


enum TradeRestType {
	trTrade,
	trOrderMove,
	trOrderCancel,
	trOrderTrigger,
	trOrderOk,
	trOrderDelayed,
	trOrderNoBudget
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

	/**
	 * @param buyOrder buy order (original, not updated)
	 * @param sellOrder sell order (original, not updated)
	 * @param fullBuy true if order has been fully executed
	 * @param fullSell true if order has been fully executed
	 * @param size size of the trade
	 * @param price price of the trade
	 * @param dir action of the taker (buy = taker is buy, sell = taker is sell)
	 */
	TradeResultTrade(
				POrder buyOrder,
				POrder sellOrder,
				bool fullBuy,
				bool fullSell,
				std::size_t size,
				std::size_t price,
				OrderDir::Type dir
				):AbstractTradeResult(trTrade)
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

class TradeResultOrderDelayStatus: public AbstractTradeResult {
public:
	TradeResultOrderDelayStatus(POrder order)
		:AbstractTradeResult(trOrderDelayed),order(order) {}


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

class TradeResultOrderNoBudget: public AbstractTradeResult {
public:
	TradeResultOrderNoBudget(POrder order, std::size_t price)
		:AbstractTradeResult(trOrderNoBudget),order(order),price(price) {}


	const POrder& getOrder() const {
		return order;
	}

	std::size_t getPrice() const {
		return price;
	}

protected:
	POrder order;
	std::size_t price;

};



typedef json::RefCntPtr<AbstractTradeResult> PTradeResult;

} /* namespace quark */


