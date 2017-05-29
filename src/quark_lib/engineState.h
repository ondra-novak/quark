#pragma once

#include <unordered_map>
#include <imtjson/json.h>
#include <queue>
#include <set>

#include "ITradeResult.h"



#include "order.h"

namespace quark {

class EngineState: public json::RefCntObj {
public:

	typedef json::Value Value;


	EngineState(Value id, json::RefCntPtr<EngineState> prevState);



	struct OrderUpdate {
		POrder oldOrder;
		POrder newOrder;
	};


	typedef std::unordered_map<json::Value, OrderUpdate> StateRecord;


	StateRecord &getChanges() {return record;}
	const StateRecord &getChanges() const {return record;}

	json::RefCntPtr<EngineState> getPrevState() const {return prevState;}

	std::size_t getLastPrice() const {
		return lastPrice;
	}

	void setLastPrice(std::size_t lastPrice) {
		this->lastPrice = lastPrice;
	}

	Value getStateId() const {return stateId;}

	void erasePrevState() {prevState = nullptr;}

protected:

	Value stateId;
	StateRecord record;
	json::RefCntPtr<EngineState> prevState;
	std::size_t lastPrice;
};

typedef json::RefCntPtr<EngineState> PEngineState;


typedef json::Value OrderId;

enum TxAction {
	actionAddOrder,
	actionUpdateOrder,
	actionRemoveOrder

};


struct TxItem {
	TxAction action;
	OrderId orderId;
	POrder order;
};


typedef std::function<void(const ITradeResult &)> Output;

typedef json::StringView<TxItem> Transaction;


class OrderCompare {
public:
	typedef bool (*Fn)(const POrder &, const POrder &);
	OrderCompare(Fn fn):fn(fn) {}

	bool operator()(const POrder &a, const POrder &b) const {
		return fn(a,b);
	}

	Fn fn;
};


} /* namespace quark */
