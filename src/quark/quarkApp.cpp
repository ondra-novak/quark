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

#include "init.h"
#include "logfile.h"
#include "orderRangeError.h"

namespace quark {

const StrViewA QuarkApp::marketConfigDocName("settings");
const StrViewA QuarkApp::FIELD_STATUS("status");

QuarkApp::QuarkApp() {
	// TODO Auto-generated constructor stub

}

void QuarkApp::processOrder(Value cmd) {

	Document orderDoc(cmd);

	if (marketCfg == nullptr) {
		orderDoc(FIELD_STATUS, "rejected")
		   ("error",Object("message","market is not opened yet"))
		   ("finished",true);
		ordersDb->put(orderDoc);
	}

	//TODO process orders in paralel

	if (orderDoc["finished"].getBool()) return;

	StrViewA status = orderDoc[FIELD_STATUS].getString();




	if (status == "created") {
		logInfo({"Create order", orderDoc.getIDValue()});
		createOrder(orderDoc);

	} else if (status == "ok") {
		if (orderDoc["updateReq"].defined()) {

			logInfo({"Update order", orderDoc.getIDValue()});
			updateOrder(orderDoc);
		} else {
			logInfo({"Execute order", orderDoc.getIDValue()});
			matchOrder(orderDoc);

		}

	}

}

void QuarkApp::updateOrder(Document order) {


	Value req = order["updateReq"];
	if (req["status"] == "canceled") {
		if (coreState.isKnownOrder(order.getIDValue())) {
			TxItem txi;
			txi.action = actionRemoveOrder;
			txi.orderId = order.getIDValue();
			runTransaction(txi);
		}
		saveOrder(order, Object("updateReq",json::undefined)
						("status",req["status"])
						("blocked",json::undefined)
						("finished",true));

		updateUserBudget(order["user"]);
	} else {


		Object changes;
		for (Value v : req) {
			changes.set(v);
		}
		changes.set("updateReq",json::undefined);
		changes.set("updateStatus","ok");
		Document d = saveOrder(order, changes);
		try {
			updateUserBudget(order["user"]);
			TxItem txi;
			txi.action = actionUpdateOrder;
			txi.orderId = d.getIDValue();
			txi.order = docOrder2POrder(d);
		} catch (OrderErrorException &e) {

			Object undoChanges;
			for (Value v : req) {
				changes.set(order[v.getKey()]);
			}
			changes.set("updateReq",json::undefined);
			changes.set("updateStatus","error");
			changes.set("updateError",e.what());
			saveOrder(d, changes);
		}

	}



}

Document QuarkApp::saveOrder(Document order, Object newItems) {

		newItems.setBaseObject(order);

		try {
			Value v(newItems);
			logDebug(v);
			Document d(v);
			ordersDb->put(d);
			return d;
		} catch (UpdateException &e) {
			if (e.getError(0).isConflict()) {
				order = ordersDb->get(order.getID());
				saveOrder(order,newItems);
			}
		}

}

void QuarkApp::matchOrder(Document& order) {

	if (coreState.isKnownOrder(order.getID())) return;
	TxItem txi;
	txi.action = actionAddOrder;
	txi.orderId = order.getIDValue();
	txi.order = docOrder2POrder(order);

	runTransaction(txi);

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


void QuarkApp::runTransaction(const TxItem& txitm) {
	transactionCounter++;
	coreState.matching(transactionCounter,Transaction(&txitm,1), [&](const ITradeResult &res){
		//TODO

	});
}

void QuarkApp::updateUserBudget(Value user) {

	View budget("_design/users/_view/budget",View::groupLevel|View::update|View::reduce);

	Query q = ordersDb->createQuery(budget);
	Result r = q.key(user).exec();
	if (r.empty()) {
		logInfo({"Clearing budget for user", user});
	} else {
		logInfo({"Allocating budget for user", user, Row(r[0]).value});
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

void QuarkApp::createOrder(Document order) {

		POrder po;
		try {

			po = docOrder2POrder(order);
		} catch (OrderRangeError &e) {
		saveOrder(order,
				Object(FIELD_STATUS, "rejected")
									("error",Object("code", e.getCode())
												  ("message",e.getMessage())
												  ("rangeValue",e.getRangeValue()))
									("finished",true)
									);
			return;
		} catch (OrderErrorException &e) {

		saveOrder(order,
				Object(FIELD_STATUS, "rejected")
									("error",Object("code", e.getCode())
												  ("message",e.getMessage()))
									("finished",true)
									);


			return;
		}


		BlockedBudget budget = calculateBudget(order);

		saveOrder(order, Object("blocked", budget.toJson())
							   (FIELD_STATUS, "ok"));

		try {
			updateUserBudget(order["user"]);
		} catch (OrderErrorException &e) {

			saveOrder(order,
					Object(FIELD_STATUS, "rejected")
										("error",Object("code", e.getCode())
													  ("message",e.getMessage()))
										("finished",true)
										);
			return;
		}


}



void QuarkApp::mainloop() {

	View queueView("_design/orders/_view/queue", View::includeDocs|View::update);

	auto loopBody = [&](ChangedDoc chdoc) {

		try {

			if (!chdoc.deleted)  {
				if (chdoc.id == marketConfigDocName) {
					try {
						marketCfg = new MarketConfig(chdoc.doc);
						logInfo("Market configuration updated");
					} catch (std::exception &e) {
						logError({"MarketConfig update failed", e.what()});
					}
				} else if (chdoc.id.substr(0,8) != "_design/"){
					processOrder(chdoc.doc);
				}
			}
		} catch (std::exception &e) {
			logError({"Unhandled exception in mainloop",e.what()});
		}
		return true;
	};

	Query q = ordersDb->createQuery(queueView);
	Result res = q.exec();
	for (Value v : res) {
		loopBody(v);
	}


	ChangesFeed chfeed = ordersDb->createChangesFeed();
	exitFn = [&] {
		chfeed.cancelWait();
	};

	try {
		chfeed.setFilter(queueView).since(res.getUpdateSeq()).setTimeout(-1) >> loopBody;
	} catch (CanceledException &e) {

	}

}


void QuarkApp::receiveMarketConfig() {
	Value doc = ordersDb->get(marketConfigDocName, CouchDB::flgNullIfMissing);
	if (doc != nullptr) {
		marketCfg = new MarketConfig(doc);
	}
}



void QuarkApp::start(couchit::Config cfg) {





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


	receiveMarketConfig();

	mainloop();


}


} /* namespace quark */
