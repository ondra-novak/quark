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


Value findTradeCounter(couchit::CouchDB& tradeDB, Value trade) {
	Document d = tradeDB.get(trade.getString());
	return d["index"];
}


void resync(couchit::CouchDB& ordersDB, couchit::CouchDB& tradeDB,
		ITradeStream &moneySrvClient, const Value fromTrade, const Value toTrade,
		const MarketConfig &mcfg) {

	Query q = tradeDB.createQuery(View::includeDocs);
	if (fromTrade != nullptr) {
		q.range(fromTrade, "Z");
	} else {
	    q.range("A","Z");
	}
	Result r = q.exec();
	Value lastTradeId = fromTrade;
	//sends reports to money server(s)
	for (Row v : r) {
		if (v.id == fromTrade) continue;
		Value t = v.doc;
		IMoneySrvClient::TradeData td;
		Query q = ordersDB.createQuery(View::includeDocs);
		q.keys({t["buyOrder"],t["sellOrder"]}).update();
		Result ores = q.exec();
		if (ores.size() != 2) {
			throw std::runtime_error(String({"Order not found for trade:" , v.id.getString(),}).c_str());
		}
		extractTrade(t, Row(ores[0]).doc, Row(ores[1]).doc, td);
		moneySrvClient.reportTrade(lastTradeId, td);
		if (v.id == toTrade) break;
		lastTradeId = td.id;
	}
}

Value fetchLastTrade(CouchDB& tradeDB) {
	Query q = tradeDB.createQuery(View::includeDocs);
	Result res = q.reversedOrder().update().limit(1).range("0","Z").exec();
	if (res.empty()) return nullptr;
	else return Row(res[0]).doc;
}
}

quark::MoneySvcSupport::MoneySvcSupport(PCouchDB orderDB, PCouchDB tradeDB,PMarketConfig mcfg, Dispatcher &dispatcher)
	:orderDB(orderDB),tradeDB(tradeDB),mcfg(mcfg),dispatcher(dispatcher)
{
}

void quark::MoneySvcSupport::resync(ITradeStream &target, const Value fromTrade, const Value toTrade) {
	quark::resync(*orderDB, *tradeDB, target, fromTrade, toTrade, *mcfg);
}

void quark::MoneySvcSupport::dispatch(Action fn) {
	fn >> dispatcher;
}
