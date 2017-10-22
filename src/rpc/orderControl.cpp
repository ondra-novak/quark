/*
 * orderControl.cpp
 *
 *  Created on: 16. 6. 2017
 *      Author: ondra
 */

#include "orderControl.h"

#include <couchit/document.h>
#include "../quark_lib/constants.h"

namespace quark {

using namespace json;
using namespace couchit;


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


	Value x = reqOrder[OrderFields::dir];
	if (!OrderDir::str.find(x.getString())) {
			return validationError(OrderFields::dir,OrderDir::str.allEnums());
	}

	x = reqOrder[OrderFields::type];
	const OrderType::Type *t = OrderType::str.find(x.getString());
	if (!t)
			return validationError(OrderFields::type, OrderType::str.allEnums());


	x = reqOrder[OrderFields::size];
	if (x.getNumber() <= 0) {
			return validationError(OrderFields::size,
					"Zero or negative size is not allowed");
	}
		x = reqOrder[OrderFields::limitPrice];
	if (x.defined() )
	{
		if ( x.getNumber() <= 0)
				return validationError(OrderFields::limitPrice,
						"Must not be negative or zeroed");

	} else {
		if (*t == OrderType::limit || *t == OrderType::stoplimit
				|| *t == OrderType::maker
				|| *t == OrderType::ioc
				|| *t == OrderType::fok
				|| *t == OrderType::oco_limitstop) {
				return validationError(OrderFields::limitPrice, "Must be defined");
		}
	}

	x = reqOrder[OrderFields::stopPrice];
	if (x.defined() )
	{
		if ( x.getNumber() <= 0)
				return validationError(OrderFields::stopPrice,
						"Must not be negative or zeroed");

	} else {
		if (
				*t == OrderType::stop || *t == OrderType::stoplimit
				|| *t == OrderType::oco_limitstop) {
				return validationError(OrderFields::stopPrice, "Must be defined");
		}
	}

	x = reqOrder[OrderFields::trailingDistance];
	if (x.defined() )
	{
		if ( x.getNumber() <= 0)
				return validationError(OrderFields::trailingDistance,"Must not be negative or zeroed");

	}

	x = reqOrder[OrderFields::user];
	if (!x.defined() ) {
			return validationError(OrderFields::user, "must be defined");
	}
	x = reqOrder[OrderFields::context];
	if (!x.defined() ) {
			return validationError(OrderFields::context, "must be defined");
	}
	if (!OrderContext::str.find(x.getString()))
			return validationError(OrderFields::context, OrderContext::str.allEnums());
	x = reqOrder[OrderFields::finished];
	if (x.defined() ) {
		return validationError(OrderFields::finished, "must not be defined");
	}
	x = reqOrder[OrderFields::fees];
	if (x.defined() ) {
		if (x.type() != json::number) {
			if (x.type() != json::array)
				return validationError(OrderFields::fees,"must be ethier number or array of two values [maker, taker]");
			if (x.size() != 2)
				return validationError(OrderFields::fees,"must be array of two values [maker, taker]");
			if (x[0].type() != json::number)
				return validationError(OrderFields::fees,"maker fee must be number");
			if (x[1].type() != json::number)
				return validationError(OrderFields::fees,"taker fee must be number");
		}
	}
	x = reqOrder[OrderFields::budget];
	if (x.defined()) {
		if (x.type() != json::number)
			return validationError(OrderFields::budget,"must be number");
		if (x.getNumber()<0)
			return validationError(OrderFields::budget,"must not be negative");
	}
	x = reqOrder[OrderFields::queuePriority];
	if (x.defined()) {
		if (x.type() != json::number)
			return validationError(OrderFields::queuePriority,"must be number");
	}
	x = reqOrder[OrderFields::domPriority];
	if (x.defined()) {
		if (x.type() != json::number)
			return validationError(OrderFields::domPriority,"must be number");
	}

	time_t ct;
	time(&ct);

	Document doc;
	Value vid = reqOrder[OrderFields::vendorId];
	if (vid.defined()) {
		if (vid.type() != json::string)
			return validationError(OrderFields::vendorId, "must be string");
		if (vid.getString().substr(0,2) != "o.")
			return validationError(OrderFields::vendorId, "must begin with prefix 'o.' ");
		doc.setID(vid);
	} else {
		doc = db.newDocument("o.");
	}

	doc.setBaseObject(reqOrder);
	doc.set( OrderFields::status,Status::str[Status::validating])
		.set(OrderFields::timeCreated, ct)
		.set(OrderFields::origSize, reqOrder[OrderFields::size]);
	doc.unset(OrderFields::vendorId);
	doc.enableTimestamp();

	try {
		db.put(doc);
		return {doc.getIDValue(),doc.getRevValue()};
	} catch (UpdateException &e) {
		if (e.getErrors()[0].isConflict()) {
			throw ConflictError(normFields(db.get(doc.getID())));
		}
		throw;
	}

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
	if (revId.defined() && doc.getRevValue() != revId) {
		normFields(doc);
		throw ConflictError(doc);
	}

	return doc;
}

json::Value OrderControl::modify(json::Value orderId, json::Value reqOrder, json::Value revId) {
	Document doc = loadOrder(orderId, revId);
	for (Value v : reqOrder) {
		StrViewA x = v.getKey();
			if (x == "" || x[0] == '_'
				|| x == OrderFields::dir
				|| x == OrderFields::type
				|| x == OrderFields::status
				|| x == OrderFields::updateReq
				|| x == OrderFields::updateStatus
				|| x == OrderFields::user
				|| x == OrderFields::context
				|| x == OrderFields::fees) {
			return validationError(x,"The field is read-only");
		}
		if (!doc[x].defined()) {
			return validationError(x,"The field doesn't exists");
		}
		if (v.type() != doc[x].type()) {
			return validationError(x,"The field has different type");
		}
			if ((x == OrderFields::limitPrice || x == OrderFields::stopPrice
					|| x == OrderFields::trailingDistance)
					&& v.getNumber() <= 0) {
			return validationError(x,"Zero or negative value is not allowed");
		}
	}

	doc.set(OrderFields::updateReq, reqOrder)
			(OrderFields::updateStatus,Status::str[Status::validating]);

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
	doc.set(OrderFields::cancelReq,true);
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
	normFields(doc);
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

json::Value quark::OrderControl::normFields(json::Value data) {
	Object res(data);
	normFields(res);
	return res;
}

void quark::OrderControl::normFields(json::Object& data) {
	Value id = data["_id"];
	Value rev = data["_rev"];
	data.set("id", id)
		   ("rev",rev)
		   ("_id",json::undefined)
		   ("_rev",json::undefined);

}


}


