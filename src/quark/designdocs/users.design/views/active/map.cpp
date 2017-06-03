#include <couchcpp/api.h>

void mapdoc(Document doc) {

if (doc["status"] != "finished" && doc["status"] != "canceled") {
	emit(doc["user"], doc["_rev"]);
}

}
