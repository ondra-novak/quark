#include <couchcpp/api.h>

void mapdoc(Document doc) {
	  if (doc.getID().substr(0,2) == "t.") {
		  auto tm = doc["time"].getUInt();
		  emit({
			  tm/604800,
			  (tm/86400)%7,
			  (tm/14400)%6,
			  (tm/3600)%4,
			  (tm/900)%4,
			  (tm/300)%3,
			  (tm/60)%5
		  }, {doc["price"],doc["size"],doc["index"]});
	}
}
