#include "tradeHelpers.h"



#include <couchit/couchDB.h>
#include <couchit/document.h>
#include <couchit/query.h>
#include "views.h"




#include "../quark_lib/constants.h"


namespace quark {
using namespace couchit;
using namespace json;


void extractTrade(const couchit::Value& trade,
		IMoneySrvClient::TradeData& tdata) {

	tdata.dir = OrderDir::str[trade["dir"].getString()];
	tdata.id = trade["_id"];
	tdata.price = trade["price"].getNumber();
	tdata.size = trade["size"].getNumber();
	tdata.timestamp = trade["time"].getUInt();
	tdata.nonce = trade["index"].getUInt();

}


static double calcFee(const Document &order, double total, bool taker) {

	Value f = order[OrderFields::fees];
	if (f.type() == number) {
		return total * f.getNumber();
	} else if (f.type() == array) {
		int idx = taker?OrderFields::takerFee:OrderFields::makerFee;
		return total * f[idx].getNumber();
	} else {
		return 0;
	}

}

void extractBalanceChange(couchit::CouchDB& orderDB,
		const couchit::Value& trade, IMoneySrvClient::BalanceChange& tdata,
		OrderDir::Type dir,const MarketConfig &mcfg) {


	Value orderId = trade[dir == OrderDir::buy ? "buyOrder" : "sellOrder"];
	Document order = orderDB.get(orderId.getString());
	double size = trade["size"].getNumber();
	double price = trade["price"].getNumber();
	double total = mcfg.adjustTotal(size*price);
	tdata.assetChange = dir == OrderDir::buy ? size : -size;
	tdata.currencyChange = dir == OrderDir::buy ? -total : total;
	tdata.context = OrderContext::str[order[OrderFields::context].getString()];
	tdata.fee = mcfg.adjustTotal(
					calcFee(order, total, OrderDir::str[trade["dir"].getString()] == dir)
					);
	tdata.user = order[OrderFields::user];
	tdata.trade = trade["_id"];

}


Value findTradeCounter(couchit::CouchDB& tradeDB, Value trade) {
	Document d = tradeDB.get(trade.getString());
	return d["index"];
}


void resync(couchit::CouchDB& ordersDB, couchit::CouchDB& tradeDB,
		PMoneySrvClient moneySrvClient, const Value fromTrade, const Value toTrade,
		const MarketConfig &mcfg) {

	Query q = tradeDB.createQuery(tradesByCounter);
	q.includeDocs();
	q.update();
	if (fromTrade != nullptr) {
		Value cntv = findTradeCounter(tradeDB,fromTrade);
		q.range(cntv, json::undefined);
	}
	Result r = q.exec();
	Value lastTradeId = fromTrade;
	//sends reports to money server(s)
	for (Row v : r) {
		if (v.id == fromTrade) continue;
		if (v.id == toTrade) break;
		IMoneySrvClient::TradeData td;
		IMoneySrvClient::BalanceChange bch;
		extractTrade(v.doc, td);
		Value clt = moneySrvClient->reportTrade(lastTradeId, td);
		if (clt != td.id) {
			//We can lost connection during synchronization
			//in this case, we need to repeat synchronization
			return resync(ordersDB, tradeDB, moneySrvClient, clt, toTrade,mcfg);
		}
		extractBalanceChange(ordersDB,v.doc,bch,OrderDir::buy,mcfg);
		moneySrvClient->reportBalanceChange(bch);
		extractBalanceChange(ordersDB,v.doc,bch,OrderDir::sell,mcfg);
		moneySrvClient->reportBalanceChange(bch);
		moneySrvClient->commitTrade(td.id);
		lastTradeId = td.id;
	}
}

Value fetchLastTrade(CouchDB& tradeDB) {
	Query q = tradeDB.createQuery(tradesByCounter);
	Result res = q.reversedOrder().includeDocs().update().limit(1).exec();
	if (res.empty()) return nullptr;
	else return Row(res[0]).doc;
}
}

