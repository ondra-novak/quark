#include <unordered_map>
#include "tradeHelpers.h"



#include <couchit/couchDB.h>
#include <couchit/document.h>
#include <couchit/query.h>
#include "views.h"
#include "logfile.h"




#include "../quark_lib/constants.h"


namespace quark {
using namespace couchit;
using namespace json;


void extractTrade(const couchit::Value& trade,
		const couchit::Value &buyorder,
		const couchit::Value &sellorder,
		IMoneySrvClient::TradeData& tdata) {

	tdata.dir = OrderDir::str[trade["dir"].getString()];
	tdata.id = trade["_id"];
	tdata.price = trade["price"].getNumber();
	tdata.size = trade["size"].getNumber();
	tdata.timestamp = trade["time"].getUInt();
	tdata.buyer.context = buyorder[OrderFields::context];
	tdata.buyer.userId = buyorder[OrderFields::user];
	tdata.seller.context= sellorder[OrderFields::context];
	tdata.seller.userId= sellorder[OrderFields::user];
}

void extractTrade2(const couchit::Value& trade,
		IMoneySrvClient::TradeData& tdata) {

	tdata.dir = OrderDir::str[trade["dir"].getString()];
	tdata.id = trade["_id"];
	tdata.price = trade["price"].getNumber();
	tdata.size = trade["size"].getNumber();
	tdata.timestamp = trade["time"].getUInt();
	Value buy = trade["buy"];
	tdata.buyer.context = buy[2].getBool()?OrderContext::strMargin:OrderContext::strExchange;
	tdata.buyer.userId = buy[1];
	Value sell = trade["sell"];
	tdata.seller.context= sell[2].getBool()?OrderContext::strMargin:OrderContext::strExchange;
	tdata.seller.userId= sell[1];
}




Value findTradeCounter(couchit::CouchDB& tradeDB, Value trade) {
	Document d = tradeDB.get(trade.getString());
	return d["index"];
}


void resync(couchit::CouchDB& ordersDB, couchit::CouchDB& tradeDB,
		ITradeStream &moneySrvClient, Value fromTrade, Value toTrade,
		const MarketConfig &mcfg) {

	bool rep;
	Array orders;
	do {
		Query q = tradeDB.createQuery(View::includeDocs);
		if (fromTrade != nullptr) {
			q.range(fromTrade, "Z");
		} else {
			q.range("A","Z");
		}
		q.limit(10000);
		Result r = q.exec();

		logInfo({"SYNC: loaded trades", r.size()});

		Query qo = ordersDB.createQuery(View::includeDocs);
		orders.clear();
		std::unordered_map<Value, Value> orderMap;
		for (Row v : r) {
			if (v.doc["buy"].defined()) continue;

			Value t = v.doc["buyOrder"];
			if (orderMap.insert(std::make_pair(t,Value())).second == true) {
				orders.push_back(t);
			}
			t = v.doc["sellOrder"];
			if (orderMap.insert(std::make_pair(t,Value())).second == true) {
				orders.push_back(t);
			}
		}
		if (!orders.empty()) {
			Result ro = qo.keys(orders).exec();

			logInfo({"SYNC: loaded orders", ro.size()});

			for (Row v : ro) {
				orderMap[v.id] = v.doc;
			}
		}


		Value lastTradeId = fromTrade;
		rep = false;
		//sends reports to money server(s)
		for (Row v : r) {
			if (v.id == fromTrade) continue;
			rep = true;
			Value t = v.doc;
			IMoneySrvClient::TradeData td;
			if (t["buy"].defined()) {//new format
				extractTrade2(t, td);
			} else {
				Value buyOrder = t["buyOrder"];
				Value sellOrder = t["sellOrder"];
				buyOrder = orderMap[buyOrder];
				sellOrder = orderMap[sellOrder];
				if (!buyOrder.defined()) {
					logError(String({"Sync reference integrity error: Buy order ",String(t["buyOrder"])," was not found in the database. Trade: " , v.id.getString()}).c_str());
					if (v.id == toTrade) break;
					continue;
				}
				if (!sellOrder.defined()) {
					logError(String({"Sync reference integrity error: Sell order ",String(t["sellOrder"])," was not found in the database. Trade: " , v.id.getString()}).c_str());
					if (v.id == toTrade) break;
					continue;
				}
				extractTrade(t, buyOrder,sellOrder, td);
			}
			moneySrvClient.reportTrade(lastTradeId, td);
			lastTradeId = td.id;
			if (v.id == toTrade) break;
		}
		fromTrade = lastTradeId;
	} while (rep);
}

Value fetchLastTrade(CouchDB& tradeDB) {
	Query q = tradeDB.createQuery(View::includeDocs);
	Result res = q.reversedOrder().update().limit(1).range("0","Z").exec();
	if (res.empty()) return nullptr;
	else return Row(res[0]).doc;
}
}
