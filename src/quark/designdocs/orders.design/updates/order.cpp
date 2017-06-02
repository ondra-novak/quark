/*
 * order.cpp
 *
 *  Created on: Jun 1, 2017
 *      Author: ondra
 */

void validationError(StrViewA field, StrViewA message) {

	start(Object("Content-Type","application/json"),400);
	sendJSON(Object("status","error")
			("field",field)
			("message", message));

}

void update(Document &doc, Value req) {

	if (req["method"] == "PUT") {
		if (doc.isNull()) {
			start(Object(),404);
			send("Order not found");
		} else {

			/* update order */


		}
	} else {

		if (doc.isNull()) {
		} else {
			start(Object("Allow","PUT"),405);
			send("Method not allowed");

		}

		Object newOrder;
		Value reqOrder = Value::fromString(req["body"].getString());

		Value x = reqOrder["dir"];
		if (x.getString() != "buy" && x.getString() != "sell") {
			return validationError("dir","Mandatory field must either 'buy' or 'sell'");
		}
		newOrder.set(x);

		x = reqOrder["type"];
		StrViewA xtype = x.getString();
		if (xtype != "limit"
			&& xtype != "market"
			&& xtype != "stop"
			&& xtype != "stoplimit"
			&& xtype != "postlimit"
			&& xtype != "ioc"
			&& xtype != "fok"
			&& xtype != "trailing-stop"
			&& xtype != "trailing-stoplimit"
			&& xtype != "trailing-limit"
			) return validationError("type","Unknown order type");

		newOrder.set(x);

		x = reqOrder["size"];
		if (x.getNumber() <= 0) {
			return validationError("size","Zero or negative size is not allowed");
		}
		newOrder.set(x);
		x = reqOrder["limitPrice"];
		if (x.defined() )
		{
			if ( x.getNumber() <= 0)
					return validationError("limitPrice","Must not be negative or zeroed");
			newOrder.set(x);

		} else {
			if (
					xtype == "limit" || xtype == "stoplimit"
							|| xtype == "postlimit"
									|| xtype == "ioc"
											|| xtype == "fok"
													|| xtype == "trailing-limit"
															|| xtype == "trailing-stoplimit"){
			return validationError("limitPrice","Must be defined");
			}
		}

		x = reqOrder["stopPrice"];
		if (x.defined() )
		{
			if ( x.getNumber() <= 0)
					return validationError("stopPrice","Must not be negative or zeroed");
			newOrder.set(x);

		} else {
			if (!x.defined() && (
					xtype == "stop" || xtype == "stoplimit"
						|| xtype == "stop"
						|| xtype == "trailing-stop"
						|| xtype == "trailing-stoplimit")) {
			return validationError("stopPrice","Must be defined");
			}
		}

		x = reqOrder["trailingDistance"];
		if (x.defined() )
		{
			if ( x.getNumber() <= 0)
					return validationError("trailingDistabce","Must not be negative or zeroed");
			newOrder.set(x);

		} else {
			if (x.defined() && (x.getNumber() <= 0
						|| xtype == "trailing-stop"
						|| xtype == "trailing-stoplimit"
						|| xtype == "trailing-limit")) {
			return validationError("trailingDistance","Must be defined");
			}
		}

		x = reqOrder["user"];
		if (!x.defined() ) {
			return validationError("user","must be defined");
		}

		newOrder.set(reqOrder["user"]);
		newOrder.set("starus","created");
		newOrder.set("_id",req["uuid"]);


		doc = Value(newOrder);


		start(Object("Content-Type","application/json"),200);
		sendJSON(Object("status","ok")("orderId",newOrder["_id"]));



	}


}


