#include <couchcpp/api.h>

bool filter(Document doc, Value req) {
	return doc.getID() == "error";
}
