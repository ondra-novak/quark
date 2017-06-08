
/*
 * order.cpp
 *
 *  Created on: Jun 1, 2017
 *      Author: ondra
 */

StrViewA FIELD__ID = "_id";
StrViewA FIELD_STATUS = "status";
StrViewA FIELD_USER = "user";
StrViewA FIELD_TRAILING_DISTANCE = "trailingDistance";
StrViewA FIELD_STOP_PRICE = "stopPrice";
StrViewA FIELD_LIMIT_PRICE = "limitPrice";
StrViewA FIELD_SIZE = "size";
StrViewA FIELD_TYPE = "type";
StrViewA FIELD_DIR = "dir";
StrViewA SELL = "sell";
StrViewA BUY = "buy";
StrViewA TRAILING_STOPLIMIT = "trailing-stoplimit";
StrViewA TRAILING_LIMIT = "trailing-limit";
StrViewA TRAILING_STOP = "trailing-stop";
StrViewA FOK = "fok";
StrViewA IOC = "ioc";
StrViewA POSTLIMIT = "postlimit";
StrViewA STOPLIMIT = "stoplimit";
StrViewA STOP = "stop";
StrViewA MARKET = "market";
StrViewA LIMIT = "limit";
StrViewA FIELD_CANCELED = "canceled";
StrViewA  FIELD_UPDATE_REQ = "updateReq";
StrViewA FIELD_TMCREATED = "createTime";

void validationError(StrViewA field, StrViewA message) {

	start(Object("Content-Type","application/json"),400);
	sendJSON(Object(FIELD_STATUS, "error")
			("field",field)
			("message", message));

}

void okResponse(Document& doc) {
	start(Object("Content-Type", "application/json"), 200);
	sendJSON(Object(FIELD_STATUS, "ok")("orderId", doc[FIELD__ID]));
}

void update(Document &doc, Value req) {

	if (req["method"].getString() == "GET") {
		start(Object("Allow","POST,PUT,DELETE"),405);
		send("Method not allowed");
		return;

	} else if (req["method"].getString() == "DELETE") {

		if (doc.isNull()) {
			start(Object(),404);
			send("Order not found");
		} else {
			doc = doc.replace(FIELD_UPDATE_REQ,
					Object(FIELD_STATUS, FIELD_CANCELED));
			okResponse(doc);
		}
	} else {
		Value reqOrder ;
		try {
			reqOrder = Value::fromString(req["body"].getString());
		} catch (std::exception &e) {
			return validationError("POST-BODY",e.what());
		}


	if (req["method"].getString() == "PUT") {
		if (doc.isNull()) {
			start(Object(),404);
			send("Order not found");
		} else {

			for (Value v : reqOrder) {
				StrViewA x = v.getKey();
					if (x == FIELD_DIR || x == FIELD_TYPE || x == FIELD_STATUS
							|| x == FIELD__ID || x == FIELD_UPDATE_REQ
							|| x == FIELD_USER) {
					return validationError(x,"The field is read-only");
				}
				if (!doc[x].defined()) {
					return validationError(x,"The field doesn't exists");
				}
				if (v.type() != doc[x].type()) {
					return validationError(x,"The field has different type");
				}
					if ((x == FIELD_LIMIT_PRICE || x == FIELD_STOP_PRICE
							|| x == FIELD_TRAILING_DISTANCE)
							&& v.getNumber() <= 0) {
					return validationError(x,"Zero or negative value is not allowed");
				}
			}

				doc = doc.replace(FIELD_UPDATE_REQ, reqOrder).
						replace("updateStatus","validation");

			okResponse(doc);

		}
	} else if (req["method"].getString() == "POST") {

		if (doc.isNull()) {
		} else {
			start(Object("Allow","PUT"),405);
			send("Method not allowed");
			return;
		}


		Value x = reqOrder[FIELD_DIR];
			if (x.getString() != BUY && x.getString() != SELL) {
				return validationError(FIELD_DIR,
						"Mandatory field must either 'buy' or 'sell'");
		}

			x = reqOrder[FIELD_TYPE];
		StrViewA xtype = x.getString();
			if (xtype != LIMIT
			&& xtype != MARKET
			&& xtype != STOP
			&& xtype != STOPLIMIT
			&& xtype != POSTLIMIT
			&& xtype != IOC
			&& xtype != FOK
			&& xtype != TRAILING_STOP
			&& xtype != TRAILING_STOPLIMIT
			&& xtype != TRAILING_LIMIT
			)
				return validationError(FIELD_TYPE, "Unknown order type");


		x = reqOrder[FIELD_SIZE];
		if (x.getNumber() <= 0) {
				return validationError(FIELD_SIZE,
						"Zero or negative size is not allowed");
		}
			x = reqOrder[FIELD_LIMIT_PRICE];
		if (x.defined() )
		{
			if ( x.getNumber() <= 0)
					return validationError(FIELD_LIMIT_PRICE,
							"Must not be negative or zeroed");

		} else {
			if (
					xtype == LIMIT || xtype == STOPLIMIT
							|| xtype == POSTLIMIT
						|| xtype == IOC
											|| xtype == FOK
						|| xtype == TRAILING_LIMIT
						|| xtype == TRAILING_STOPLIMIT) {
					return validationError(FIELD_LIMIT_PRICE, "Must be defined");
			}
		}

			x = reqOrder[FIELD_STOP_PRICE];
		if (x.defined() )
		{
			if ( x.getNumber() <= 0)
					return validationError(FIELD_STOP_PRICE,
							"Must not be negative or zeroed");

		} else {
			if (
					xtype == STOP || xtype == STOPLIMIT
						|| xtype == STOP
						|| xtype == TRAILING_STOP
						|| xtype == TRAILING_STOPLIMIT) {
					return validationError(FIELD_STOP_PRICE, "Must be defined");
			}
		}

			x = reqOrder[FIELD_TRAILING_DISTANCE];
		if (x.defined() )
		{
			if ( x.getNumber() <= 0)
					return validationError(FIELD_TRAILING_DISTANCE,"Must not be negative or zeroed");

		} else {
				if (xtype == TRAILING_STOP
				|| xtype == TRAILING_STOPLIMIT
						|| xtype == TRAILING_LIMIT) {
					return validationError(FIELD_TRAILING_DISTANCE,
							"Must be defined");
			}
		}

			x = reqOrder[FIELD_USER];
		if (!x.defined() ) {
				return validationError(FIELD_USER, "must be defined");
		}

		time_t t;
		time(&t);

		Object obj(reqOrder);
			obj(FIELD_STATUS, "validation")
		   (FIELD__ID, String({"o.",req["uuid"].getString()}))
		   (FIELD_TMCREATED, t);


		doc = Value(obj);


		okResponse(doc);



	}
	}

}


