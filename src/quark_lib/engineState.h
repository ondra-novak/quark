#pragma once

#include <unordered_map>
#include <imtjson/json.h>


#include "order.h"

namespace quark {

class EngineState: public json::RefCntObj {
public:

	typedef json::Value Value;

	EngineState(Value id, Value prevState);



	struct OrderUpdate {
		POrder oldOrder;
		POrder newOrder;
	};


	typedef std::unordered_map<json::Value, OrderUpdate> StateRecord;


protected:

	Value stateId;
	StateRecord record;
	Value prevState;
};

typedef json::RefCntPtr<EngineState> PEngineState;

} /* namespace quark */


