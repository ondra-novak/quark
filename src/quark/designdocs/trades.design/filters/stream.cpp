#include <couchcpp/api.h>

bool filter(Document doc, Value request) {
	return doc.getID().substr(0,2) == "t." && doc["_deleted"].getBool() == false;
}
