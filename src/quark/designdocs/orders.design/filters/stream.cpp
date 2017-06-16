#include <couchcpp/api.h>

bool filter(Document doc, Value req) {
	return doc.getID().substr(0,2) == "o." && doc["_deleted"].getBool() == false;
}
