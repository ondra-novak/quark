#include <couchcpp/api.h>

void mapdoc(Document doc) {

	if (doc["_id"].getString().substr(0,2) == "o.") {
		if (doc["finished"].getBool() == false) {
			Value b = doc["blocked"];
			emit(doc["user"], b);
		}
	}
}
