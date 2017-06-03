#include <couchcpp/api.h>

void mapdoc(Document doc) {

if (doc["finished"].getBool() != true && doc["user"].defined()) {
	emit(doc["user"], doc["_rev"]);
}

}
