#include <couchcpp/api.h>

struct Data {

	double open, high, low, close, volume,sum, sum2, volume2;
	std::uintptr_t count;
	std::uintptr_t index;


};


Value composeData(const Data &x) {
	return {
		x.open, x.high,x.low,x.close,x.volume, x.count, x.sum, x.sum2, x.volume2, x.index
	};

}

Value reduce(RowSet rows) {
	//open=0,high=1, low=2, close=3, volume=4, n=5 , sum=6, sum2=7, volume2=8, index=9  - use indexes for performance reason

	Data x;
	auto iter = rows.begin(), end = rows.end();
	x.close = x.open = x.low = x.high = (*iter).value[0].getNumber();
	x.volume = (*iter).value[1].getNumber();
	x.index = (*iter).value[2].getNumber();
	x.sum = x.close;
	x.sum2 = x.sum*x.sum;
	x.volume2 = x.volume;
	x.count=1;

	auto openIndex = x.index, closeIndex = x.index;
	++iter;
	while (iter != end) {
		double price = (*iter).value[0].getNumber();
		double volume = (*iter).value[1].getNumber();
		std::uintptr_t index = (*iter).value[2].getUInt();

		if (openIndex > index) {
			x.open = price;
			x.index = index;
			openIndex = index;
		}
		if (closeIndex < index) {
			x.close = price;
			closeIndex = index;
		}
		x.high = std::max(x.high, price);
		x.low = std::min(x.low, price);
		x.volume += volume;
		x.volume2 += volume*volume;
		x.sum += price;
		x.sum2 += price*price;
		++x.count;
		++iter;
	}

	return composeData(x);
}

void extractValue(const Value& v, Data& x) {
	x.open = v[0].getNumber();
	x.high = v[1].getNumber();
	x.low = v[2].getNumber();
	x.close = v[3].getNumber();
	x.volume = v[4].getNumber();
	x.count = v[5].getUInt();
	x.sum = v[6].getNumber();
	x.sum2 = v[7].getNumber();
	x.volume2 = v[8].getNumber();
	x.index = v[8].getUInt();
}

Value rereduce(Value values) {

	Data x;
	auto iter = values.begin(), end = values.end();
	extractValue(*iter, x);
	auto openIndex = x.index, closeIndex = x.index;
	++iter;
	while (iter != end) {
		Data y;
		extractValue(*iter, y);

		if (openIndex > y.index) {
			x.open = y.open;
			x.index = y.index;
			openIndex = y.index;
		}
		if (closeIndex < y.index) {
			x.close = y.close;
			closeIndex = y.index;
		}
		x.high = std::max(x.high, y.high);
		x.low = std::min(x.low, y.low);
		x.volume += y.volume;
		x.volume2 += y.volume2;
		x.sum += y.sum;
		x.sum2 += y.sum2;
		x.count += y.count;
		++iter;
	}

	return composeData(x);

}
