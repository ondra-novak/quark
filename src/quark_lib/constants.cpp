#include "constants.h"

namespace quark {


#define ENUMDEF(x) { x, #x }
#define STRCONST(x) json::StrViewA x(#x)

namespace OrderDir {

json::NamedEnum<Type> str(
		{
	ENUMDEF(buy),
	ENUMDEF(sell)
		}
);

}

namespace OrderType {


json::NamedEnum<Type> str(
		{
		ENUMDEF(market),
		ENUMDEF(limit),
		ENUMDEF(maker),
		ENUMDEF(stop),
		ENUMDEF(stoplimit),
		ENUMDEF(fok),
		ENUMDEF(ioc),
		ENUMDEF(trailingStop),
		ENUMDEF(trailingLimit),
		ENUMDEF(trailingStopLimit)}
);
}

namespace OrderFields {

STRCONST(orderId);
STRCONST(dir);
STRCONST(type);
STRCONST(size);
STRCONST(limitPrice);
STRCONST(stopPrice);
STRCONST(trailingDistance);
STRCONST(domPriority);
STRCONST(queuePriority);
STRCONST(budget);
STRCONST(context);
STRCONST(user);
STRCONST(status);
STRCONST(error);
STRCONST(finished);
STRCONST(fees);
STRCONST(updateReq);
STRCONST(updateStatus);
STRCONST(timeCreated);
STRCONST(timeModified);


}

namespace Status {

json::NamedEnum<Type> str({
	ENUMDEF(validating),
	ENUMDEF(active),
	ENUMDEF(finished),
	ENUMDEF(rejected),
	ENUMDEF(canceled),
	ENUMDEF(accepted)}
);
}

namespace OrderContext {
json::NamedEnum<Type> str({
		ENUMDEF(exchange),
		ENUMDEF(margin)
	});
}

}


