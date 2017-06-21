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

QuarkApp::QuarkApp() {
	// TODO Auto-generated constructor stub

	moneyService = new ErrorMoneyService;
}

void QuarkApp::processPendingOrders(Value user) {
	Value cmd = pendingOrders.unlock(user);
	while (cmd.defined()) {
		processOrder2(cmd);
		cmd = pendingOrders.unlock(user);
	}
}

bool QuarkApp::runOrder(const Document &order, bool update) {

	Value user = order[OrderFields::user];

	//calculate budget
	auto b = calculateBudget(order);

	LOGDEBUG3("Budget calculated for the order", order.getIDValue(), b.toJson());

	if (!moneyService->allocBudget(order[OrderFields::user], order.getIDValue(), b, [=](bool response) {
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

				processPendingOrders(user);
			}
		)) return false;
	else {
		runOrder2(order,update);
		return true;
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
		changes.set(OrderFields::updateStatus,Status::strRejected)
				   (OrderFields::updateReq,json::undefined);
	} else {
		changes.set(OrderFields::status,Status::strRejected);
					(OrderFields::finished,true);
	}
	changes.set(OrderFields::error, Object("code",e.getCode())
						("message",e.getMessage())
						("rangeValue",rangeValue));
	saveOrder(order,changes);

}

void QuarkApp::runOrder2(Document order, bool update) {


	LOGDEBUG3("Executing order", order.getIDValue(), update?"(update)":"(new order)");
	//stage 2
	try {
		//create transaction item
		TxItem txi;
		txi.action = update?actionUpdateOrder:actionAddOrder;
		txi.orderId = order.getIDValue();
		txi.order = docOrder2POrder(order);


		if (order[OrderFields::status].getString() != Status::strActive) {
			try {
				order.set(OrderFields::status, Status::strActive);
				ordersDb->put(order);
			} catch (const UpdateException &e) {
				//in case of conflict...
				if (e.getErrors()[0].isConflict()) {
					logWarn({"Order creation conflict, waiting for fresh data", order.getIDValue()});
					return;
				}
				throw;
			}
		}

		//run transaction
		runTransaction(txi);


		//handle various exceptions
	 }catch (OrderRangeError &e) {
		 rejectOrder(order, e, update);
	 }

}


void QuarkApp::processOrder(Value cmd) {

	Value user = cmd[OrderFields::user];
	if (!pendingOrders.lock(user, cmd))
		return;

	processOrder2(cmd);
	processPendingOrders(user);

};

void QuarkApp::processOrder2(Value cmd) {
	Document order(cmd);
	Sync _(ordLock);

	LOGDEBUG2("Pop order", order.getIDValue());

	if (marketCfg == nullptr) {
		LOGDEBUG2("Rejected order (no market)", order.getIDValue());
		order(OrderFields::status, Status::strRejected)
		   (OrderFields::error,Object("message","market is not opened yet"))
		   ("finished",true);
		ordersDb->put(order);
		return;
	}

	//skip finished orders
	if (order["finished"].getBool()) {
		LOGDEBUG2("Skipped finished order", order.getIDValue());
		return;
	}

	///process update on order - false = no update found
	if (!checkUpdate(order)) {

		//if order is not part of matching - it is probably new or after the restart
		if (!coreState.isKnownOrder(order.getIDValue())) {
			//run order (may run async)
			runOrder(order, false);

		}

	}
}

bool QuarkApp::checkUpdate(Document order) {

	bool isKnown = coreState.isKnownOrder(order.getIDValue());

	if (order[OrderFields::cancelReq].getBool()) {
		//request to cancel order
		LOGDEBUG2("Order is user canceled", order.getIDValue());
		if (isKnown) {
			//remove order from market
			TxItem txi;
			txi.action = actionRemoveOrder;
			txi.orderId = order.getIDValue();
			runTransaction(txi);
		}
		//unblocks the blocked budget by this command
		//this can be done asynchronously without waiting
		moneyService->allocBudget(order[OrderFields::user],order.getIDValue(), zeroBudget(order), nullptr);
		//save the order
		saveOrder(order, Object(OrderFields::cancelReq,json::undefined)
						(OrderFields::status,Status::strCanceled)
						("finished",true));

		return true;
	} else {

		Value req = order[OrderFields::updateReq];
		if (!req.defined()) return false;


		LOGDEBUG3("Order is updated by user", order.getIDValue(), order[OrderFields::updateReq]);

		for (Value v : req) {
			order.set(v);
		}
		order.set(OrderFields::updateReq,json::undefined);
		order.set(OrderFields::updateStatus,"accepted");

		try {
			ordersDb->put(order);
		} catch (const UpdateException &e) {
			if (e.getErrors()[0].isConflict()) {
				logWarn({"Order update conflict, waiting for fresh data", order.getIDValue()});
				return true;
			}
			throw;
		}

		runOrder(order, isKnown);

		return true;
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



OrderBudget QuarkApp::calculateBudget(const Document &order) {

	OrderDir::Type dir = OrderDir::str[order[OrderFields::dir].getString()];
	Value size = order[OrderFields::size];
	double slippage = 1+ marketCfg->maxSlippagePtc/100.0;
	OrderContext::Type context = OrderContext::str[order[OrderFields::context].getString()];
	if (dir == OrderDir::buy) {

		Value budget = order[OrderFields::budget];

		Value limPrice = order[OrderFields::limitPrice];
		if (limPrice.defined()) {
			double dbudget = size.getNumber()*limPrice.getNumber();

			if (budget.defined())dbudget = std::min(dbudget, budget.getNumber());

			return OrderBudget(context, OrderBudget::currency,dbudget).adjust(*marketCfg);
		}
		Value stopPrice = order["stopPrice"];
		if (stopPrice.defined()) {
			double dbudget = stopPrice.getNumber()*size.getNumber()*slippage;

			if (budget.defined()) dbudget = std::min(dbudget, budget.getNumber());

			return OrderBudget(context, OrderBudget::currency,dbudget).adjust(*marketCfg);
		}

		if (budget.defined()) {
			return OrderBudget(context, OrderBudget::currency,budget.getNumber()).adjust(*marketCfg);
		}

		return OrderBudget(context, OrderBudget::currency,marketCfg->pipToPrice(coreState.getLastPrice())*slippage);
	} else {

		return OrderBudget(context, OrderBudget::asset, size.getNumber());

	}


}

OrderBudget QuarkApp::zeroBudget(const Document &order) {

	OrderDir::Type dir = OrderDir::str[order[OrderFields::dir].getString()];
	OrderContext::Type context = OrderContext::str[order[OrderFields::context].getString()];
	if (dir == OrderDir::buy) {
		return OrderBudget(context, OrderBudget::currency,0);
	} else {
		return OrderBudget(context, OrderBudget::asset, 0);

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
				moneyService->allocBudget(d[OrderFields::user],d.getIDValue(),budget,nullptr);
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
								(OrderFields::status,Status::strExecuted);
					 else
						buyOrder("size",marketCfg->sizeToAmount(t.getBuyOrder()->getSize()-t.getSize()));

					if (t.isFullSell())
						sellOrder("finished",true)
								(OrderFields::status,Status::strExecuted);
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
					if (o->getLimitPrice()) changes(OrderFields::limitPrice,marketCfg->pipToPrice(o->getLimitPrice()));
					if (o->getTriggerPrice()) changes("stopPrice",marketCfg->pipToPrice(o->getTriggerPrice()));
				}break;
			case quark::trOrderOk: {
				}break;
			case quark::trOrderCancel: {
					const quark::TradeResultOrderCancel &t = dynamic_cast<const quark::TradeResultOrderCancel &>(r);
					Document &o = o2u[t.getOrder()->getId()];
					o(OrderFields::status,"canceled")
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
		saveOrder(order,Object(OrderFields::updateReq,json::undefined)
				(OrderFields::updateStatus,Status::strRejected)
				("updateError","budget"));
	} else {
		LOGDEBUG2("Order budget rejected (new order)", order.getIDValue());
		saveOrder(order,Object(OrderFields::updateReq,json::undefined)
				(OrderFields::status,Status::strRejected)
				(OrderFields::error,"budget")
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
	if ((v = order[OrderFields::limitPrice]).defined()) {
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
		v = order[OrderFields::limitPrice];
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
			order[OrderFields::trailingDistance].getNumber());
	odata.domPriority = order[OrderFields::domPriority].getInt();
	odata.queuePriority = order[OrderFields::queuePriority].getInt();
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

			Value errorDoc = ordersDb->get(OrderFields::error, CouchDB::flgNullIfMissing);
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
			errdoc.setID(OrderFields::error);
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


bool QuarkApp::PendingOrders::lock(Value id, const Value& doc) {
	std::lock_guard<std::mutex> _(l);
	auto f = orders.find(id);
	if (f == orders.end()) {
		orders.operator [](id);
		return true;
	} else {
		orders[id].push(doc);
		return false;
	}
}

Value QuarkApp::PendingOrders::unlock(Value id) {
	std::lock_guard<std::mutex> _(l);
	auto f = orders.find(id);
	if (f == orders.end()) return undefined;
	if (f->second.empty()) {
		orders.erase(f);
		return undefined;
	}
	Value out = f->second.front();
	f->second.pop();
	return out;
}


} /* namespace quark */
