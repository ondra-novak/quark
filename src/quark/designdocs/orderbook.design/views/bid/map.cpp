/*
 * map.cpp
 *
 *  Created on: Jun 4, 2017
 *      Author: ondra
 */

#include <couchcpp/api.h>

void mapdoc(Document doc) {

	if (doc.getID().substr(0,2) == "o."
			&& doc["finished"].getBool() == false
			&& doc["dir"] == "buy"
			&& doc["status"].getString() == "active"
			&& (doc["type"].getString() == "limit"
				|| doc["type"].getString() == "postlimit")) {

		emit(doc["limitPrice"], doc["size"]);
	}

}



