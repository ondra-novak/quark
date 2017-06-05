/*
 * map.cpp
 *
 *  Created on: Jun 4, 2017
 *      Author: ondra
 */

#include <couchcpp/api.h>

void mapdoc(Document doc) {

	if (!doc["_deleted"].getBool()) {

		StrViewA name = doc.getID();
		if (name == "settings" || name == "error") {
			emit(nullptr,nullptr);
		}
		if (name.substr(0,2) == "o." && doc["finished"].getBool() == false) {
			emit(true,nullptr);
		}
	}


}



