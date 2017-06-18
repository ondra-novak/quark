/*
 * orderControl.cpp
 *
 *  Created on: 16. 6. 2017
 *      Author: ondra
 */

#include "orderControl.h"

#include <couchit/document.h>

namespace quark {

using namespace json;
using namespace couchit;

StrViewA FIELD__ID = "_id";
StrViewA FIELD_STATUS = "status";
StrViewA FIELD_USER = "user";
StrViewA FIELD_CONTEXT = "context";
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
StrViewA MARGIN = "margin";

json::Value validationError(StrViewA field, StrViewA message) {

	throw ValidatonError(Object
			("field",field)
			("message", message));
/*	start(Object("Content-Type","application/json"),400);
	sendJSON(Object(FIELD_STATUS, "error")
			("field",field)
			("message", message));
*/
	return json::undefined;
}



OrderControl::OrderControl(couchit::CouchDB& db):db(db) {
}

json::Value OrderControl::create(json::Value reqOrder) {

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

	Document doc = db.newDocument("o.");
	doc.setBaseObject(reqOrder);
	doc.set(FIELD_STATUS,"validating")
		.set(FIELD_TMCREATED, t);
	doc.enableTimestamp();
	db.put(doc);
	return {doc.getIDValue(),doc.getRevValue()};

}

Document OrderControl::loadOrder(const json::Value& orderId, const json::Value& revId) {
	StrViewA reqId = orderId.getString();
	if (reqId.substr(0, 2) != "o.") {
		throw OrderNotFoundError();
	}
	Value v = db.get(orderId.getString(), CouchDB::flgNullIfMissing);
	if (v.isNull())
		throw OrderNotFoundError();

	Document doc(v);
	if (revId.defined() && doc.getRevValue() != revId)
		throw ConflictError(doc);

	return doc;
}

json::Value OrderControl::modify(json::Value orderId, json::Value reqOrder, json::Value revId) {
	Document doc = loadOrder(orderId, revId);
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

	doc.set(FIELD_UPDATE_REQ, reqOrder)
			("updateStatus","validation");

	try {
		db.put(doc);
		return doc.getRevValue();
	} catch (const UpdateException &e) {
		if (e.getError(0).isConflict()) {
			return modify(orderId,reqOrder,revId);
		}
		throw;
	}

}

json::Value OrderControl::cancel(json::Value orderId) {
	Document doc = loadOrder(orderId,Value());
	doc.set(FIELD_UPDATE_REQ,Object(FIELD_STATUS, FIELD_CANCELED));
	try {
		db.put(doc);
		return doc.getRevValue();
	} catch (const UpdateException &e) {
		if (e.getError(0).isConflict()) {
			return cancel(orderId);
		}
		throw;
	}
}

json::Value OrderControl::getOrder(json::Value orderId) {
	Document doc = loadOrder(orderId,Value());
	return doc;
}

const char* ValidatonError::what() const throw () {
	if (s.empty()) s = error.toString();
	return s.c_str();
}

const char* ConflictError::what() const throw () {
	return "Order conflict";
}

const char* quark::OrderNotFoundError::what() const throw () {
	return "Order not found";
}


}

