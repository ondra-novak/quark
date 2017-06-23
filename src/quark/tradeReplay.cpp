/*
 * tradeReplay.cpp
 *
 *  Created on: Jun 21, 2017
 *      Author: ondra
 */

#include "tradeReplay.h"

#include <couchit/couchDB.h>
#include <couchit/query.h>

#include "views.h"

namespace quark {

TradeReplay::TradeReplay() {
	// TODO Auto-generated constructor stub

}

void TradeReplay::addSvc(PMoneyService svc) {
	svclist.push_back(svc);
}

void TradeReplay::start(couchit::CouchDB *tradeDb, couchit::CouchDB *orderDb) {

	this->orderDb = orderDb;
	this->tradeDb = tradeDb;
	couchit::ChangesFeed l = tradeDb->createChangesFeed();
	couchit::Changes chs = l.reversedOrder(true).setFilter(tradesByCounter).limit(1).setTimeout(0).exec();
	if (chs.hasItems()) {
		lastTrade = chs.getNext()["id"];
	}


	chfeed = std::unique_ptr<couchit::ChangesFeed>(new couchit::ChangesFeed(tradeDb->createChangesFeed()));

	thr = std::thread([=]{
		chfeed->setTimeout(-1).setFilter(tradesByCounter).includeDocs(true)
				 >> [=](const couchit::ChangedDoc &chdoc) {
			return worker(chdoc);
		};
	});

}

TradeReplay::~TradeReplay() {
	stop();
}

void TradeReplay::stop() {
	if (chfeed != nullptr) {
		chfeed->cancelWait();
		thr.join();
		chfeed = nullptr;
	}
}

bool TradeReplay::worker(const couchit::ChangedDoc &chdoc) {

	IMoneyService::TradeData trade;
	extractTrade(chdoc.doc, trade);
	for ( auto x : svclist) {

		Value k = x->reportTrade(lastTrade, trade);
		if (k != trade.id) {
			resync(x, k, trade.id);
		} else {
			IMoneyService::BalanceChange bch;
			extractBalanceChange(chdoc.doc, "buyOrder", bch, OrderDir::buy);
			x->reportBalanceChange(bch);
			extractBalanceChange(chdoc.doc, "sellOrder", bch, OrderDir::sell);
			x->reportBalanceChange(bch);
			x->commitTrade(trade.id);

		}


	}

}

void TradeReplay::extractTrade(const couchit::Document& trade, IMoneyService::TradeData& tdata) {

	tdata.dir = OrderDir::str[trade["dir"].getString()];
	tdata.id = trade.getIDValue();
	tdata.price = trade["price"].getNumber();
	tdata.size = trade["size"].getNumber();
	tdata.timestamp = trade["time"].getUInt();

}

void TradeReplay::extractBalanceChange(const couchit::Document& trade,StrViewA orderKey, IMoneyService::BalanceChange& tdata, OrderDir::Type dir) {
	Value orderId = trade[orderKey];
	Document order = orderDb->get(orderId.getString());
	double size = trade["size"].getNumber();
	double price = trade["price"].getNumber();
	double total = size*price;
	tdata.assetChange = dir == OrderDir::buy ? size : -size;
	tdata.currencyChange = dir == OrderDir::buy ? -total : total;
	tdata.context = OrderContext::str[order["context"].getString()];
	tdata.fee = 0; //TODO calculate fee
	tdata.user = trade["user"];
	tdata.trade = trade.getIDValue();
}

void TradeReplay::resync(PMoneyService target, const Value fromTrade, const Value toTrade) {
	couchit::Query q = tradeDb->createQuery(tradesByCounter);
	q.includeDocs();
	q.update();
	if (fromTrade != nullptr) {
		Value cntv = findTradeCounter(fromTrade);
		q.range(cntv, json::undefined);
	}
	Result r = q.exec();
	for (Row t : r) {
		IMoneyService::TradeData trade;
		extractTrade(t.doc, trade);
		IMoneyService::BalanceChange bch;
		extractBalanceChange(t.doc, "buyOrder", bch, OrderDir::buy);
		extractBalanceChange(t.doc, "sellOrder", bch, OrderDir::sell);
		if (t.id == toTrade) break;
	}
}

Value TradeReplay::findTradeCounter(Value trade) {
	Document d = tradeDb->get(trade.getString());
	return d["index"];


}

} /* namespace quark */
