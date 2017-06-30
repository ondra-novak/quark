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

	static const int takerFee = 1;
	static const int makerFee = 0;
}

namespace Status {

enum Type {
	validating,
	active,
	finished,
	rejected,
	executed,
	accepted,
	canceled
};

using json::StrViewA;


extern StrViewA strValidating;
extern StrViewA strActive;
extern StrViewA strFinished;
extern StrViewA strExecuted;
extern StrViewA strRejected;
extern StrViewA strAccepted;
extern StrViewA strCanceled;

extern json::NamedEnum<Type> str;

}


namespace OrderContext {
enum Type {
	exchange,
	margin
};

extern json::NamedEnum<Type> str;
}


}


