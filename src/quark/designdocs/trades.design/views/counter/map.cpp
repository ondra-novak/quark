#include <couchcpp/api.h>

void mapdoc(Document doc) {
	if (doc.getID().substr(0,2) == "t.") {
		emit(doc["index"], nullptr);
	}
}
