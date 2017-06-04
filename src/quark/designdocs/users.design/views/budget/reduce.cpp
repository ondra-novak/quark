#include <couchcpp/api.h>

Value reduce(RowSet rows) {
	double assets = 0;
	double currency = 0;

	for (auto x : rows) {
		assets += x.value["assets"].getNumber();
		currency += x.value["currency"].getNumber();
	}

	return Object("assets",assets)
			("currency",currency);
}
Value rereduce(Value values) {
	double assets = 0;
	double currency = 0;

	for (auto x : values) {
		assets += x["assets"].getNumber();
		currency += x["currency"].getNumber();
	}

	return Object("assets",assets)
			("currency",currency);

}

