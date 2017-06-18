/*
 * quarkApp.cpp
 *
 *  Created on: Jun 3, 2017
 *      Author: ondra
 */

#include "quarkApp.h"

#include <couchit/changes.h>

#include <couchit/document.h>
#include <couchit/query.h>
#include <couchit/changeset.h>

#include "init.h"
#include "logfile.h"
#include "mockupmoneyserver.h"
#include "orderRangeError.h"

namespace quark {

const StrViewA QuarkApp::marketConfigDocName("settings");
const StrViewA QuarkApp::FIELD_STATUS("status");

QuarkApp::QuarkApp() {
	// TODO Auto-generated constructor stub

	moneyService = new ErrorMoneyService;
}

void QuarkApp::runOrder(const Document &order, bool update) {

	//calculate budget
	auto b = calculateBudget(order);

	LOGDEBUG3("Budget calculated for the order", order.getIDValue(), b.toJson());

	//function which allocates budget and call the callback when it is finished
	auto allocBudget = [=](StdCallback cb) {
		//allocate budget and call function
		return !moneyService->allocBudget(order["user"],order.getIDValue(),
				b,[=](bool response) {
				//in positive response
				if (response) {
					//lock this object
					Sync _(ordLock);
					//go to stage 2
					runOrder2(order, update);
				} else {
					//in negative response
					//lock the object
					Sync _(ordLock);
					//reject the order
					rejectOrderBudget(order, update);
				}
				//call the callback because we are finish
				cb();
			}
		);
	};
	//allocate budget async
	if (!pendingOrders.async(order.getIDValue(), allocBudget)) {
		//if allocation done synchronously
		//go directly to stage 2
		runOrder2(order,update);
	}

}

void QuarkApp::rejectOrder(const Document &order, const OrderErrorException &e, bool update) {
	LOGDEBUG4("Order rejected", order.getIDValue(), e.what(), update?"update":"new order");
	const OrderRangeError *re = dynamic_cast<const OrderRangeError *>(&e);
	Value rangeValue;
	if (re) {
		rangeValue = re->getRangeValue();
	}

	Object changes;
	if (update) {
		changes.set("updateStatus","rejected")
				   ("updateReq",json::undefined);
	} else {
		changes.set(FIELD_STATUS,"rejected");
					("finished",true);
	}
	changes.set("error", Object("code",e.getCode())
						("message",e.getMessage())
						("rangeValue",rangeValue));
	saveOrder(order,changes);

}

void QuarkApp::runOrder2(const Document &order, bool update) {

	LOGDEBUG3("Executing order", order.getIDValue(), update?"(update)":"(new order)");
	//stage 2
	try {
		//create transaction item
		TxItem txi;
		txi.action = update?actionUpdateOrder:actionAddOrder;
		txi.orderId = order.getIDValue();
		txi.order = docOrder2POrder(order);


		if (order["status"].getString() != "active")
			saveOrder(order,Object("status","active"));

		//run transaction
		runTransaction(txi);

		//handle various exceptions
	 }catch (OrderRangeError &e) {
		 rejectOrder(order, e, update);
	 }

}


void QuarkApp::processOrder(Value cmd) {

	Document orderDoc(cmd);
	Sync _(ordLock);

	LOGDEBUG2("Pop order", orderDoc.getIDValue());

	//in case that this order is pending
	if (pendingOrders.await(orderDoc.getIDValue(), [=] {
		//schedule its processing for later
		LOGDEBUG2("Postpone", orderDoc.getIDValue());
		processOrder(ordersDb->get(orderDoc.getID()));
			}))
		//but exit now
		return;

	if (marketCfg == nullptr) {
		LOGDEBUG2("Rejected order (no market)", orderDoc.getIDValue());
		orderDoc(FIELD_STATUS, "rejected")
		   ("error",Object("message","market is not opened yet"))
		   ("finished",true);
		ordersDb->put(orderDoc);
	}

	//skip finished orders
	if (orderDoc["finished"].getBool()) {
		LOGDEBUG2("Skipped finished order", orderDoc.getIDValue());
		return;
	}

	//if order is not part of matching - it is probably new or after the restart
	if (!coreState.isKnownOrder(orderDoc.getIDValue())) {
		//run order (may run async)
		runOrder(orderDoc, false);
	}

	//order has update defined
	if (orderDoc["updateReq"].defined()) {
		//if there is pending action on the order
		if (!pendingOrders.await(orderDoc.getIDValue(), [=] {
				Sync _(ordLock);
				//perform checkUpdate after
				checkUpdate(ordersDb->get(orderDoc.getID()));
			})) {
				//perform checkUpdate now
				checkUpdate(orderDoc);
			}
	}
}

void QuarkApp::checkUpdate(Document order) {


	Value req = order["updateReq"];
	if (!req.defined()) return;

	if (req["status"] == "canceled") {
		LOGDEBUG2("Order is user canceled", order.getIDValue());
		if (coreState.isKnownOrder(order.getIDValue())) {
			TxItem txi;
			txi.action = actionRemoveOrder;
			txi.orderId = order.getIDValue();
			runTransaction(txi);
		}

		//unblocks the blocked budget by this command
		//this can be done asynchronously without waiting
		moneyService->allocBudget(order["user"],order.getIDValue(), BlockedBudget(), nullptr);
		//save the order
		saveOrder(order, Object("updateReq",json::undefined)
						("status",req["status"])
						("finished",true));

	} else {

		LOGDEBUG3("Order is updated by user", order.getIDValue(), order["updateReq"]);

		Object changes;
		for (Value v : req) {
			changes.set(v);
		}
		changes.set("updateReq",json::undefined);
		changes.set("updateStatus","accepted");

		Document d = saveOrder(order, changes);

		runOrder(d, true);
	}



}

Document QuarkApp::saveOrder(Document order, Object newItems) {

		LOGDEBUG3("Save order state", order.getIDValue(), newItems);

		newItems.setBaseObject(order);

		try {
			Value v(newItems);
			logDebug(v);
			Document d(v);
			ordersDb->put(d);
			return d;
		} catch (UpdateException &e) {
			if (e.getError(0).isConflict()) {
				LOGDEBUG2("Save order conflict", order.getIDValue());
				order = ordersDb->get(order.getID());
				saveOrder(order,newItems);
			}
		}

}



BlockedBudget QuarkApp::calculateBudget(const Document &order) {

	Value dir = order["dir"];
	Value size = order["size"];
	double slippage = 1+ marketCfg->maxSlippagePtc/100.0;
	if (dir.getString() == "buy") {

		Value budget = order["budget"];

		Value limPrice = order["limitPrice"];
		if (limPrice.defined()) {
			double dbudget = size.getNumber()*limPrice.getNumber();

			if (budget.defined())dbudget = std::min(dbudget, budget.getNumber());

			return BlockedBudget(0,dbudget).adjust(*marketCfg);
		}
		Value stopPrice = order["stopPrice"];
		if (stopPrice.defined()) {
			double dbudget = stopPrice.getNumber()*size.getNumber()*slippage;

			if (budget.defined()) dbudget = std::min(dbudget, budget.getNumber());

			return BlockedBudget(0,dbudget).adjust(*marketCfg);
		}

		if (budget.defined()) {
			return BlockedBudget(0,budget.getNumber()).adjust(*marketCfg);
		}

		return BlockedBudget(0,marketCfg->pipToPrice(coreState.getLastPrice())*slippage);
	} else {

		return BlockedBudget(size.getNumber(), 0);

	}


}


void QuarkApp::exitApp() {
	if (exitFn != nullptr)
		exitFn();
}

View budget("_design/users/_view/budget",View::groupLevel|View::update|View::reduce);


void QuarkApp::runTransaction(const TxItem& txitm) {
	transactionCounter++;
	OrdersToUpdate &o2u = o2u_1;
	coreState.matching(transactionCounter,Transaction(&txitm,1), [&](const ITradeResult &res){
		receiveResults(res,o2u);
	});

	if (o2u.empty()) return;

	Array keys;
	{
		bool rep;

		Query q = ordersDb->createQuery(View::includeDocs);
		Changeset chset = ordersDb->createChangeset();

		keys.reserve(o2u.size());
		for (auto && x: o2u) {
					keys.push_back(x.first);
		}

		do {
			q.keys(keys);
			Result res = q.exec();
			for (Row r : res) {


				Document &d = o2u[r.key];
				LOGDEBUG3("Updating order (mass)", r.key, d);
				d.setBaseObject(r.doc);
				auto budget = calculateBudget(d);
				chset.update(d);
				moneyService->allocBudget(d["user"],d.getIDValue(),budget,nullptr);
			}
			try {
				chset.commit();
				rep = false;
			} catch (UpdateException &e) {
				keys.clear();
				for (auto err : e.getErrors()) {
					if (err.isConflict()) {
						Value id = err.document["_id"];
						LOGDEBUG2("Update order conflict", id);
						o2u_2[id] = o2u_1[id];
						keys.push_back(id);
					} else {
						logError({"Unexpected error while update!", err.document, err.errorType, err.errorDetails, err.reason});
					}
				}
				std::swap(o2u_1,o2u_2);
				o2u_2.clear();
				rep = true;
			}
		}
		while (rep);
	}

}


void QuarkApp::receiveResults(const ITradeResult& r, OrdersToUpdate &o2u) {
	time_t now;
	time(&now);
	switch (r.getType()) {
			case quark::trTrade: {
					const quark::TradeResultTrade &t = dynamic_cast<const quark::TradeResultTrade &>(r);
					double price = marketCfg->pipToPrice(t.getPrice());
					double amount = marketCfg->sizeToAmount(t.getSize());
					Value dir = OrderDir::str[t.getDir()];
					logInfo({"Trade",dir,price,amount});
					Document &buyOrder = o2u[t.getBuyOrder()->getId()];
					Document &sellOrder = o2u[t.getSellOrder()->getId()];
					if (t.isFullBuy())
						buyOrder("finished",true)
								("status","executed");
					 else
						buyOrder("size",marketCfg->sizeToAmount(t.getBuyOrder()->getSize()-t.getSize()));

					if (t.isFullSell())
						sellOrder("finished",true)
								("status","executed");
					else
						sellOrder("size",marketCfg->sizeToAmount(t.getSellOrder()->getSize()-t.getSize()));
					Document trade;

					trade("_id",String({"t.", t.getBuyOrder()->getId().getString().substr(2),t.getSellOrder()->getId().getString().substr(2)}))
						 ("price",price)
						 ("buyOrder",t.getBuyOrder()->getId())
						 ("sellOrder",t.getSellOrder()->getId())
						 ("size",amount)
						 ("dir",dir)
						 ("time",(std::size_t)now);
					tradesDb->put(trade);
				}break;
			case quark::trOrderMove: {
					const quark::TradeResultOderMove &t = dynamic_cast<const quark::TradeResultOderMove &>(r);
					POrder o = t.getOrder();
					Document &changes = o2u[o->getId()];
					if (o->getLimitPrice()) changes("limitPrice",marketCfg->pipToPrice(o->getLimitPrice()));
					if (o->getTriggerPrice()) changes("stopPrice",marketCfg->pipToPrice(o->getTriggerPrice()));
				}break;
			case quark::trOrderOk: {
				}break;
			case quark::trOrderCancel: {
					const quark::TradeResultOrderCancel &t = dynamic_cast<const quark::TradeResultOrderCancel &>(r);
					Document &o = o2u[t.getOrder()->getId()];
					o("status","canceled")
					 ("finished",true)
					 ("cancel_code",t.getCode());
				}break;
			case quark::trOrderTrigger: {
					const quark::TradeResultOrderTrigger &t = dynamic_cast<const quark::TradeResultOrderTrigger &>(r);
					Document &o = o2u[t.getOrder()->getId()];
					OrderType::Type ty = t.getOrder()->getType();
					StrViewA st = OrderType::str[ty];
					o("type", st);
				}break;
			}
}

void QuarkApp::rejectOrderBudget(Document order, bool update) {
	if (update) {
		LOGDEBUG2("Order budget rejected (update)", order.getIDValue());
		saveOrder(order,Object("updateReq",json::undefined)
				("updateStatus","rejected")
				("updateError","budget"));
	} else {
		LOGDEBUG2("Order budget rejected (new order)", order.getIDValue());
		saveOrder(order,Object("updateReq",json::undefined)
				("status","rejected")
				("error","budget")
				("finished",true));
	}

}

POrder QuarkApp::docOrder2POrder(const Document& order) {
	POrder po;
	OrderJsonData odata;
	Value v;
	double x;
	odata.id = order["_id"];
	odata.dir = String(order["dir"]);
	odata.type = String(order["type"]);
	x = order["size"].getNumber();
	if (x < marketCfg->minSize)
		throw OrderRangeError(odata.id, OrderRangeError::minOrderSize,
				marketCfg->minSize);

	if (x > marketCfg->maxSize)
		throw OrderRangeError(odata.id, OrderRangeError::maxOrderSize,
				marketCfg->maxSize);

	odata.size = marketCfg->amountToSize(x);
	if ((v = order["limitPrice"]).defined()) {
		x = v.getNumber();
		if (x < marketCfg->minPrice)
			throw OrderRangeError(odata.id, OrderRangeError::minPrice,
					marketCfg->minPrice);

		if (x > marketCfg->maxPrice)
			throw OrderRangeError(odata.id, OrderRangeError::maxPrice,
					marketCfg->maxPrice);

		odata.limitPrice = marketCfg->priceToPip(x);
	} else {
		odata.limitPrice = 0;
	}
	if ((v = order["stopPrice"]).defined()) {
		x = v.getNumber();
		if (x < marketCfg->minPrice)
			throw OrderRangeError(odata.id, OrderRangeError::minPrice,
					marketCfg->minPrice);

		if (x > marketCfg->maxPrice)
			throw OrderRangeError(odata.id, OrderRangeError::maxPrice,
					marketCfg->maxPrice);

		odata.stopPrice = marketCfg->priceToPip(x);
	} else {
		odata.stopPrice = 0;
	}
	if ((v = order["budget"]).defined()) {
		x = v.getNumber();
		if (x < 0)
			throw OrderRangeError(odata.id, OrderRangeError::invalidBudget, 0);

		if (x > marketCfg->maxBudget)
			throw OrderRangeError(odata.id, OrderRangeError::outOfAllowedBudget,
					marketCfg->maxBudget);

		odata.budget = marketCfg->budgetToPip(x);
	} else {
		v = order["limitPrice"];
		if (v.defined()) {
			double expectedBudget = v.getNumber() * order["size"].getNumber();
			if (expectedBudget > marketCfg->maxBudget)
				throw OrderRangeError(odata.id,
						OrderRangeError::outOfAllowedBudget,
						marketCfg->maxBudget);
		}
		odata.budget = 0;
	}
	odata.trailingDistance = marketCfg->priceToPip(
			order["trailingDistance"].getNumber());
	odata.domPriority = order["domPriority"].getInt();
	odata.queuePriority = order["queuPriority"].getInt();
	po = new Order(odata);
	return po;
}


void QuarkApp::mainloop() {



	View queueView("_design/orders/_view/queue", View::includeDocs | View::update);
	Filter errorWait("orders/removeError");


	for (;;) {



		try {

			ChangesFeed chfeed = ordersDb->createChangesFeed();
			exitFn = [&] {
				chfeed.cancelWait();
			};

			Value errorDoc = ordersDb->get("error", CouchDB::flgNullIfMissing);
			if (!errorDoc.isNull()) {
				logError("Error is signaled, engine stopped - please remove error file to continue");

				try {
					chfeed.setFilter(errorWait).since(ordersDb->getLastKnownSeqNumber()).setTimeout(-1)
							>> [](ChangedDoc chdoc) {
						if (chdoc.deleted) return false;
						return true;
					};
				} catch (CanceledException &e) {
					return;
				}
				continue;

			}


			logInfo("==== Entering to main loop ====");



			auto loopBody = [&](ChangedDoc chdoc) {

				if (!chdoc.deleted) {
					if (chdoc.id == marketConfigDocName) {
						try {
							marketCfg = new MarketConfig(chdoc.doc);
							logInfo("Market configuration updated");
						} catch (std::exception &e) {
							logError( {	"MarketConfig update failed", e.what()});
						}
					} else if (chdoc.id.substr(0,8) != "_design/") {
						processOrder(chdoc.doc);
					}
				}
				return true;
			};

			logInfo("==== Preload commands ====");

			Query q = ordersDb->createQuery(queueView);
			Result res = q.exec();
			for (Value v : res) {
				loopBody(v);
			}

			logInfo("==== Inside of main loop ====");


			try {
				chfeed.setFilter(queueView).since(res.getUpdateSeq()).setTimeout(-1)
						>> loopBody;
			} catch (CanceledException &e) {

			}

			logInfo("==== Leaving main loop ====");

			return;

		} catch (std::exception &e) {
			logError( { "Unhandled exception in mainloop", e.what() });
			Document errdoc;
			errdoc.setID("error");
			errdoc.set("what", e.what());
			errdoc.enableTimestamp();
			ordersDb->put(errdoc);
		}

		logInfo("==== Restart main loop ====");


	}


}


void QuarkApp::receiveMarketConfig() {
	Value doc = ordersDb->get(marketConfigDocName, CouchDB::flgNullIfMissing);
	if (doc != nullptr) {
		marketCfg = new MarketConfig(doc);
	}
}


void QuarkApp::start(couchit::Config cfg, PMoneyService moneyService) {





	String dbprefix = cfg.databaseName;
	cfg.databaseName = dbprefix + "orders";
	ordersDb = std::unique_ptr<CouchDB>(new CouchDB(cfg));
	initOrdersDB(*ordersDb);

	cfg.databaseName = dbprefix + "trades";
	tradesDb = std::unique_ptr<CouchDB>(new CouchDB(cfg));
	initTradesDB(*tradesDb);

	cfg.databaseName = dbprefix + "positions";
	positionsDb = std::unique_ptr<CouchDB>(new CouchDB(cfg));
	initPositionsDB(*positionsDb);

	this->moneyService = moneyService;


	receiveMarketConfig();

	mainloop();


}


} /* namespace quark */
