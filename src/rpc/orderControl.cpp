/*
 * orderControl.cpp
 *
 *  Created on: 16. 6. 2017
 *      Author: ondra
 */

#include "orderControl.h"

#include <couchit/document.h>
#include <imtjson/fnv.h>
#include <random>
#include "fnv128.h"
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
	x = reqOrder[OrderFields::expireTime];
	if (x.defined()) {
		if (x.type()!= json::number)
			return validationError(OrderFields::expireTime,"must be number");

		x = reqOrder[OrderFields::expireAction];
		if (x.defined()){
			if (x.getString() != "cancel" && x.getString() != "market")
				return validationError(OrderFields::expireTime,"must contain either 'cancel' or 'market'");
		}
	}
	x = reqOrder[OrderFields::nonce];
	Value nonce_hash;
	Value vid;
	if (x.defined()) {
		StrViewA nonce = x.getString();
		if (x.type() != json::string || nonce.empty())
			return validationError(OrderFields::nonce, "must be non-empty string");

		std::uintptr_t nhash;
		{
			FNV1a<sizeof(nhash)> h(nhash);
			for(auto &c: nonce) h(c);
		}
		nonce_hash = nhash;
		Arith128 idhash;
		FNV128 fnv128(idhash);
		for (auto c: StrViewA(reqOrder[OrderFields::user].toString())) fnv128(c);
		for (auto c: nonce) fnv128(c);
		String sid(30, [&](char *buff) {
			int pos = 0;
			buff[pos++] = 'o';
			buff[pos++] = '.';
			base64url->encodeBinaryValue(BinaryView(reinterpret_cast<unsigned char *>(&idhash),sizeof(idhash)),[&](StrViewA fraq){
				for (auto c: fraq) {
					if (!isalnum(c)) c = 'A'+pos-2;
					buff[pos++] = c;
				}
			});
			return pos;
		});
		if (nonce.substr(0,6) == "!test:" && nonce.length<=10) {
			std::random_device rnd;
			nonce_hash = rnd();
		}
		vid = sid;

	}




	time_t ct;
	time(&ct);

	Document doc;
	if (vid.defined()) {
		doc.setID(vid);
	} else {
		doc = db.newDocument("o.");
	}

	doc.setBaseObject(reqOrder);
	doc.set( OrderFields::status,Status::str[Status::validating])
		.set(OrderFields::timeCreated, ct)
		.set(OrderFields::origSize, reqOrder[OrderFields::size])
		.set(OrderFields::nonce, json::undefined)
		.set(OrderFields::nonce_hash, nonce_hash);
	doc.enableTimestamp();

	try {
		db.put(doc);
		return {doc.getIDValue(),doc.getRevValue()};
	} catch (UpdateException &e) {
		if (e.getErrors()[0].isConflict()) {
			Value actualDoc = db.get(doc.getID());
			if (actualDoc[OrderFields::user] == doc[OrderFields::user] && actualDoc[OrderFields::nonce_hash] == doc[OrderFields::nonce_hash])
				throw ConflictError(normFields(actualDoc));
			else
				throw ConflictError(nullptr);
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
	if (doc[OrderFields::cancelReq].getBool() || doc[OrderFields::finished].getBool()) {
		return doc.getRevValue();
	} else {
		doc.set(OrderFields::cancelReq,true);
	}
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


