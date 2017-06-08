/*
 * reduce.cpp
 *
 *  Created on: Jun 9, 2017
 *      Author: ondra
 */

#include <couchcpp/api.h>

Value reduce(RowSet rows) {
	json::maxPrecisionDigits = 9;

	double amount = 0;
	unsigned int count = rows.size();;
	for (auto &&x : rows) {
		amount = amount + x.value.getNumber();
	}

	return Object("size", amount)("offers", count);
}

Value rereduce(Value rows) {
	double amount = 0;
	unsigned int count = 0;
	for (auto x : rows) {
		amount = amount + x["size"].getNumber();
		count = count + x["offers"].getUInt();
	}
	return Object("size", amount)("offers", count);

}

