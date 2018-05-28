#pragma once
#include <couchit/document.h>
#include <couchit/couchDB.h>

namespace quark {
class OrderControl {
public:
	OrderControl(couchit::CouchDB &db);


	json::Value create(json::Value v);
	json::Value modify(json::Value orderId, json::Value orderData, json::Value revId);
	json::Value cancel(json::Value orderId);
	json::Value getOrder(json::Value orderId);

	static json::Value normFields(json::Value data);
	static void normFields(json::Object &data);


protected:

	couchit::CouchDB &db;

private:
	couchit::Document loadOrder(const json::Value& orderId, const json::Value& revId);
};


class ValidatonError: public std::exception {
public:
	ValidatonError(json::Value error):error(error) {}

	json::Value getError() const {
		return error;
	}

	virtual const char *what() const throw();


protected:
	mutable json::String s;
	json::Value error;
};

class ConflictError: public std::exception {
public:

	ConflictError(const json::Value &actualDoc):actualDoc(actualDoc) {}

	json::Value getActualDoc() const {
		return actualDoc;
	}

	virtual const char *what() const throw();


protected:
	json::Value actualDoc;

};

class OrderNotFoundError: public std::exception {
public:
	virtual const char *what() const throw();
};

}
