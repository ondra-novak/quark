#pragma once

#include <imtjson/namedEnum.h>

namespace quark {


namespace OrderDir {
enum Type {
	buy,
	sell
};

extern json::NamedEnum<Type> str;


}


namespace OrderType {

enum Type {
	market,
	limit,
	maker,
	stop,
	stoplimit,
	fok,
	ioc,
	oco_limitstop
};


extern json::NamedEnum<Type> str;
}



namespace OrderFields {

	using json::StrViewA;

	extern StrViewA orderId;
	extern StrViewA dir;
	extern StrViewA type;
	extern StrViewA size;
	extern StrViewA origSize;
	extern StrViewA limitPrice;
	extern StrViewA stopPrice;
	extern StrViewA trailingDistance;
	extern StrViewA domPriority;
	extern StrViewA queuePriority;
	extern StrViewA budget;
	extern StrViewA context;
	extern StrViewA user;
	extern StrViewA status;
	extern StrViewA error;
	extern StrViewA cancelReq;
	extern StrViewA finished;
	extern StrViewA fees;
	extern StrViewA updateReq;
	extern StrViewA updateStatus;
	extern StrViewA timeCreated;
	extern StrViewA timeModified;
	extern StrViewA queue;
	extern StrViewA vendorId;
	extern StrViewA nextOrder;
	extern StrViewA prevOrder;
	extern StrViewA data;
	extern StrViewA expireTime;
	extern StrViewA expireAction;
	extern StrViewA expired;
	extern StrViewA fs;
	extern StrViewA fp;


}

namespace NextOrderFields {


using json::StrViewA;

	extern StrViewA target;
	extern StrViewA stoploss;
	extern StrViewA orderId;
	extern StrViewA size;
	extern StrViewA absolute;


}

namespace Status {

enum Type {
	validating,
	active,
	finished,
	rejected,
	executed,
	accepted,
	canceled,
	delayed
};

using json::StrViewA;


extern StrViewA strValidating;
extern StrViewA strActive;
extern StrViewA strFinished;
extern StrViewA strExecuted;
extern StrViewA strRejected;
extern StrViewA strAccepted;
extern StrViewA strCanceled;
extern StrViewA strDelayed;

extern json::NamedEnum<Type> str;

}


namespace OrderContext {
using json::StrViewA;

extern StrViewA strExchange;
extern StrViewA strMargin;

enum Type {
	exchange,
	margin
};

extern json::NamedEnum<Type> str;
}


}


