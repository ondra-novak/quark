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
#include <couchit/couchDB.h>
#include <couchit/query.h>
#include "../common/config.h"
#include "../common/runtime_error.h"
#include "../common/semaphore.h"

#include "error.h"


#include "init.h"
#include "logfile.h"
#include "marginTradingSvc.h"
#include "mockupmoneyserver.h"
#include "moneyServerClient2.h"
#include "orderRangeError.h"
#include "tradeHelpers.h"
#include "views.h"
#include "../common/lexid.h"


namespace quark {

const StrViewA QuarkApp::marketConfigDocName("settings");
const StrViewA QuarkApp::errorDocName("error");
const StrViewA QuarkApp::controlDocName("control");

QuarkApp::QuarkApp():rnd(std::random_device()()) {
	controlServer.add("stop",this,&QuarkApp::controlStop);
	controlServer.add("dumpState",this,&QuarkApp::controlDumpState);
	controlServer.add("dumpBlocked",this,&QuarkApp::controlDumpBlocked);
	controlServer.add_ping("ping");
	controlServer.add_listMethods("listMethods");

}


bool QuarkApp::runOrder(Document order, bool update) {

	try {
		PQuarkApp me(this);
		Value user = order[OrderFields::user];

		//calculate budget
		auto b = calculateBudget(order);

		//this value limits budget on order when buying
		double bl = b.getBudgetOrderLimit();

		LOGDEBUG3("Budget calculated for the order", order.getIDValue(), b.toJson());

		if (!moneyService->allocBudget(order[OrderFields::user], order.getIDValue(), b,
							[me,order,update,user,bl](IMoneySrvClient::AllocResult response) {
				QuarkApp *mptr = me;

				//probably exit, do not process the order
				if (me->moneyService == nullptr) {
					return;

				}
				//in positive response
				switch (response) {
				case IMoneySrvClient::allocOk:
					mptr->runOrder2(order, update,bl);
					break;
				case IMoneySrvClient::allocReject:
					mptr->rejectOrderBudget(order, update);
					break;
				case IMoneySrvClient::allocError:
					mptr->rejectOrder(order,OrderErrorException(order.getIDValue(),
							OrderErrorException::internalError,
							"Failed to allocate budget (money server error)"),update);
					break;
				case IMoneySrvClient::allocTryAgain:
					return;
				}




		})) return false;
		else {
			runOrder2(order,update,bl);
			return true;
		}
	} catch (std::exception &e) {
		rejectOrder(order,
				OrderErrorException(order.getIDValue(),OrderErrorException::internalError,
						e.what()),false);
		return true;
	}

}


void QuarkApp::freeBudget(const Document& order) {
	moneyService->allocBudget(order[OrderFields::user],
			order[OrderFields::orderId], OrderBudget(), nullptr);
}

void QuarkApp::rejectOrder(Document order, const OrderErrorException &e, bool update) {
	LOGDEBUG4("Order rejected", order.getIDValue(), e.what(), update?"update":"new order");
	const OrderRangeError *re = dynamic_cast<const OrderRangeError *>(&e);
	Value rangeValue;
	if (re) {
		rangeValue = re->getRangeValue();
	}

	try {

		if (update) {
			order(OrderFields::updateStatus,Status::strRejected)
			     (OrderFields::updateReq,json::undefined);
		} else {
			order(OrderFields::status,Status::strRejected)
				 (OrderFields::finished,true);
		}
		order(OrderFields::error, Object("code",e.getCode())
								("message",e.getMessage())
								("rangeValue",rangeValue));
		ordersDb->put(order);
	} catch (UpdateException &e) {
		if (e.getErrors()[0].isConflict()) {
			logInfo({"Order was not rejected because has been changed", e.getErrors()[0].document});
			return;
			}
		throw;
	}
	if (!update) {
		freeBudget(order);
	}
}

void QuarkApp::runOrder2(Document order, bool update, double marketBuyBudget) {


	LOGDEBUG3("Executing order", order.getIDValue(), update?"(update)":"(new order)");
	//stage 2
	try {
		//create transaction item
		TxItem txi;
		txi.action = update?actionUpdateOrder:actionAddOrder;
		txi.orderId = order.getIDValue();
		txi.order = docOrder2POrder(order,marketBuyBudget);


		if (order[OrderFields::status].getString() != Status::strActive) {
			try {
				Value limPrice = order[OrderFields::limitPrice];
				Value stopPrice = order[OrderFields::stopPrice];
				Value distance = order[OrderFields::trailingDistance];
				Value size = order[OrderFields::size];
				Value orgSize = order[OrderFields::origSize];

				//normalize values on order
				if (limPrice.defined())
					order.set(OrderFields::limitPrice, marketCfg->adjustPrice(limPrice.getNumber()));
				if (stopPrice.defined())
					order.set(OrderFields::stopPrice, marketCfg->adjustPrice(stopPrice.getNumber()));
				if (distance.defined())
					order.set(OrderFields::trailingDistance, marketCfg->adjustPrice(distance.getNumber()));
				if (size.defined())
					order.set(OrderFields::size, marketCfg->adjustSize(size.getNumber()));
				if (orgSize.defined())
					order.set(OrderFields::origSize, marketCfg->adjustSize(orgSize.getNumber()));

				order.set(OrderFields::status, Status::strActive);

				ordersDb->put(order);
				//when order is put with expireTime
				if (order[OrderFields::expireTime].defined()) {
					//wake-up scheduler to reschedule
					schedulerIntr.push(true);
				}
			} catch (const UpdateException &e) {
				//in case of conflict...
				if (e.getErrors()[0].isConflict()) {
					logWarn({"Order has changed - removing from the queue without matching", order.getIDValue()});
					return;
				}
				throw;
			}
		} else {
			//check for any change happened during waiting to approval
			//change is detected by comparing revisions
			//This is achieved by querying _all_docs where key is ID
			//the revision is returned as value under key "rev"
			//if the revision is different, the code stops here
			//... because if the change has been caused by a user, the order will
			//be processed again (or has been already processed)
			Result res = ordersDb->createQuery(0).key(order.getIDValue()).exec();
			Row rw = res[0];
			if (rw.value["rev"] != order.getRevValue()) {
				logWarn({"Order has changed - removing from the queue without matching", order.getIDValue()});
				return;
			}
		}

		//run transaction
		runTransaction(txi);



		//handle various exceptions
	 }catch (OrderRangeError &e) {
		 rejectOrder(order, e, update);
	 } catch (OrderErrorException &e) {
		 rejectOrder(order,e,update);
	 }catch (std::exception &e) {
		 rejectOrder(order, OrderErrorException(order.getIDValue(),OrderErrorException::internalError,e.what()),update);
	 }

}



void QuarkApp::execControlOrder(Value cmd) {

	try {
		Value req = cmd["request"];
		if (req.defined()) {
			json::RpcRequest rpcreq = json::RpcRequest::create(req,
					[=](Value resp) {

				Document doc(cmd);
				doc(OrderFields::finished,true)
				   (OrderFields::status,Status::strExecuted)
				   ("response",resp);

				 try {
					 ordersDb->put(doc);
				 } catch (UpdateException &e) {
					 logError({"Unable to update control order (success)", doc});
				 }

			});

			controlServer(rpcreq);
		}


	} catch (std::exception &e) {
		Document doc(cmd);
		doc(OrderFields::finished,true)
		   (OrderFields::status,Status::strRejected)
		   (OrderFields::error,Object("code", 400)("message",e.what()));
		 try {
			 ordersDb->put(doc);
		 } catch (UpdateException &e) {
			 logError({"Unable to update control order (failure)", doc});
		 }
	}

}



void QuarkApp::processOrder(Value cmd) {


	Value type = cmd[OrderFields::type];
	if (type == "control") {
		execControlOrder(cmd);
		return;

	}
	
	processOrder2(cmd);

};

bool QuarkApp::isUpdated(const Document &order) {
	return order[OrderFields::updateReq].defined();

}
bool QuarkApp::isCanceled(const Document &order) {
	return order[OrderFields::cancelReq].getBool();
}


bool QuarkApp::processOrder2(Value cmd) {
	Document order(cmd);



	LOGDEBUG2("Pop order", order.getIDValue());

	if (marketCfg == nullptr) {
		LOGDEBUG2("Rejected order (no market)", order.getIDValue());
		order(OrderFields::status, Status::strRejected)
		   (OrderFields::error,Object("message","market is not opened yet"))
		   (OrderFields::finished,true);
		ordersDb->put(order);
		return true;
	}

	//skip finished orders
	if (order[OrderFields::finished].getBool()) {
		LOGDEBUG2("Skipped finished order", order.getIDValue());
		return true;
	}

	if (isCanceled(order)) {
		cancelOrder(order);
		return true;
	}
	if (isUpdated(order)) {
		return updateOrder(order);
	}
	if (coreState.isKnownOrder(order.getIDValue())) {
		return true;
	}
	return runOrder(order, false);
}
void QuarkApp::cancelOrder(Document order) {
	Value orderId = order.getIDValue();
	bool isKnown = coreState.isKnownOrder(orderId);
	//request to cancel order
	LOGDEBUG2("Order is user canceled", orderId);

	order.unset(OrderFields::cancelReq);
	order.set(OrderFields::status, Status::strCanceled);
	order.set(OrderFields::finished, true);

	try {
		ordersDb->put(order);
	} catch (UpdateException &e) {
		if (e.getErrors()[0].isConflict()) {
			logInfo({"Order was not canceled because it changed",e.getErrors()[0].document});
		}
		return;
	}


	if (isKnown) {
		//remove order from market
		TxItem txi;
		txi.action = actionRemoveOrder;
		txi.orderId = orderId;
		runTransaction(txi);
	}
	//unblocks the blocked budget by this command
	//this can be done asynchronously without waiting
	freeBudget(order);
}

bool QuarkApp::updateOrder(Document order) {
	Value orderId = order.getIDValue();
	bool isKnown = coreState.isKnownOrder(orderId);
	Value req = order[OrderFields::updateReq];

	LOGDEBUG3("Order is updated by user", orderId, req);

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

	return runOrder(order, isKnown);
}

double QuarkApp::estimateMarketBuy(double size) {
	auto c = coreState.estimateBudgetForMarket(OrderDir::buy, marketCfg->assetToSize(size));
	if (c == 0) return lastPrice*size;
	else return marketCfg->budgetFromFixPt(c);
}

OrderBudget QuarkApp::calculateBudget(const Document &order) {

	if (order[OrderFields::finished].getBool()) return OrderBudget();

	OrderDir::Type dir = OrderDir::str[order[OrderFields::dir].getString()];
	double size = marketCfg->adjustSize(order[OrderFields::size].getNumber());

	double slippage = 1+ marketCfg->maxSlippagePct/100.0;
	OrderContext::Type context = OrderContext::str[order[OrderFields::context].getString()];
	if (dir == OrderDir::sell && context != OrderContext::margin) {
		return OrderBudget(size,0);
	} else {

		double reqbudget;
		Value budget = order[OrderFields::budget];
		if (budget.defined()) {
			reqbudget = budget.getNumber();
		} else {
			OrderType::Type otype = OrderType::str[order[OrderFields::type].getString()];
			switch (otype) {
			case OrderType::maker:
			case OrderType::stoplimit:
			case OrderType::limit:
			case OrderType::ioc:
			case OrderType::oco_limitstop:
			case OrderType::fok:
				reqbudget = size * order[OrderFields::limitPrice].getNumber();
				break;
			case OrderType::stop:
				//use bigger budget (for market or for stopprice)
				reqbudget = std::max(estimateMarketBuy(size), order[OrderFields::stopPrice].getNumber() * size) * slippage;
				break;
			default:
				reqbudget = estimateMarketBuy(size) * slippage;
				break;
			}
		}

		if (context == OrderContext::margin) {
			if (dir == OrderDir::sell)
				return OrderBudget(0,reqbudget,0,size).adjust(*marketCfg);
			else
				return OrderBudget(reqbudget,0,size,0).adjust(*marketCfg);
		} else {
			return OrderBudget(0, reqbudget).adjust(*marketCfg);
		}
	}

}



void QuarkApp::exitApp() {
	dispatcher.quit();
}



void QuarkApp::runTransaction(const TxItem& txitm) {
	transactionCounter++;
	OrdersToUpdate &o2u = o2u_1;
	tradeList.clear();
	o2u.clear();
	Changeset chset = tradesDb->createChangeset();
	try {
		coreState.matching(transactionCounter,Transaction(&txitm,1), [&](const ITradeResult &res){
			receiveResults(res,o2u,tradeList);
		});
	} catch (OrderErrorException &e) {
		Value order = ordersDb->get(e.getOrderId().getString(), CouchDB::flgNullIfMissing);
		if (order.isNull()) {
			logError({"Unknown order exception", e.getOrderId(), e.getCode(), e.getMessage()});
		} else {
			Document doc(order);
			rejectOrder(order,e,false);
		}
		return;
	}


	if (!tradeList.empty())
	{

		//commit all trades into DB
		//build list of trades to commit
		for (auto && v: tradeList) {
			chset.update(v);
		}
		//commit in one call (any possible conflicts are thrown here before orders are
		//sent to the money server
		chset.commit();

		tradesDb->updateView(userTrades,false);


		//send trades to the transaction module
		for (auto && v : tradeList) {
			IMoneySrvClient::TradeData td;

			extractTrade2(v, td);
			moneySrvClient->reportTrade(lastTradeId, td);

			//update last tradeId
			lastTradeId = td.id;
			//update last price
			lastPrice = td.price;

		}
	}
	//update all affected orders
	queryKeysArray.clear();
	if (!o2u.empty())
	{
		bool rep;

		//prepare query
		Query q = ordersDb->createQuery(View::includeDocs);
		//load all keys to the query
		for (auto && x: o2u) {
					queryKeysArray.push_back(x.first);
		}

		do {
			//query all changed orders
			q.keys(queryKeysArray);
			Result res = q.exec();
			if (res.size() != queryKeysArray.size())
				throw std::runtime_error("Matching engine refers some orders not found in the database - sanity check");
			//process results
			for (Row r : res) {

				//perform update
				Document d = o2u[r.key];
				LOGDEBUG3("Updating order (mass)", r.key, d);
				//the map contains only changes. Now set the base object onto the changes will be applied
				d.setBaseObject(r.doc);

				d.optimize();

				//if order is unknown for the core, check what happened
				if (!coreState.isKnownOrder(r.key)) {

					bool finished = d[OrderFields::finished].getBool();
					//if order is not market as finished, but it hash defined custom budget
					if (!finished && d[OrderFields::budget].defined()) {
						//then we cannot continue in this order, so mark it as finished
						d.set(OrderFields::finished, true);
						//mark with status executed
						d.set(OrderFields::status, Status::executed);
						//set finished flag
						finished = true;
					}

					//we run out of budget and order is not finished yet
					if (!finished) {
						//delete order from the budget, we will reacquire it later
						moneyService->deleteOrder(d[OrderFields::user],d.getIDValue());
						//this enqueues request to download specified order and
						//process it like by queue.
						String orderId (r.key);
						[orderId,this]{
							Value cmd = ordersDb->get(orderId, CouchDB::flgNullIfMissing);
							if (cmd != nullptr) {
								if (cmd[OrderFields::status].getString() == Status::strActive) {
									processOrder(cmd);
								}
							} //send to the dispatcher
						} >> dispatcher;


					} else {
						//in case that order is no longer in matching
						//remove order's budget from the money service
						//in case that order will be requeued, it will ask for budget again later
						moneyService->allocBudget(d[OrderFields::user],d.getIDValue(),OrderBudget(),nullptr);

						if (d[OrderFields::finished].getBool() &&  !d[OrderFields::nextOrder].empty()) {
							createNextOrder(d, d[OrderFields::nextOrder]);
						}

					}



				} else {
					//update budget
					OrderBudget b = calculateBudget(d);
					moneyService->allocBudget(d[OrderFields::user],d.getIDValue(),b,nullptr);
				}


				//put order for update
				chset.update(d);
			}
			try {
				//commit all order change
				chset.commit(*ordersDb);
				//in case of all success, we end here
				rep = false;


			} catch (UpdateException &e) {
				//there can be conflicts
				queryKeysArray.clear();
				//because changes from trading have highest priority
				//we have to reapply changes to new version of the order
				for (auto err : e.getErrors()) {
					if (err.isConflict()) {
						//collect conflicts
						Value id = err.document["_id"];
						LOGDEBUG2("Update order conflict", id);
						o2u_2[id] = o2u_1[id];
						//create new query
						queryKeysArray.push_back(id);
					} else {
						//other exceptions are not allowed here
						throw;
					}
				}
				//swap maps
				std::swap(o2u_1,o2u_2);
				//clear other map
				o2u_2.clear();
				//repeat
				rep = true;
			}
		}
		while (rep);
	}
	//everything done, let get next order

}


void QuarkApp::receiveResults(const ITradeResult& r, OrdersToUpdate &o2u, TradeList &trades) {
	std::uniform_int_distribution<std::size_t> uniform_dist(1, 65536);
	time_t now;
	time(&now);
	switch (r.getType()) {
			case quark::trTrade: {
					const quark::TradeResultTrade &t = dynamic_cast<const quark::TradeResultTrade &>(r);
					double price = marketCfg->currencyToPrice(t.getPrice());
					double amount = marketCfg->sizeToAsset(t.getSize());
					std::size_t buyRemain;
					std::size_t sellRemain;
					Value dir = OrderDir::str[t.getDir()];

					auto tbo = t.getBuyOrder();
					auto tso = t.getSellOrder();
					const Value &tbd = tbo->getData();
					const Value &tsd = tso->getData();

					Document &buyOrder = o2u[tbo->getId()];
					Document &sellOrder = o2u[tso->getId()];
					buyOrder(OrderFields::size,marketCfg->sizeToAsset(buyRemain = tbo->getSize()-t.getSize()));
					sellOrder(OrderFields::size,marketCfg->sizeToAsset(sellRemain = tso->getSize()-t.getSize()));
					if (buyRemain == 0 || tbd[1].getBool()) {
							buyOrder(OrderFields::finished,true)
									(OrderFields::status,Status::strExecuted);
					}else {
							buyOrder(OrderFields::status,Status::strActive);
					}

					if (sellRemain == 0 || tsd[1].getBool()) {
							sellOrder(OrderFields::finished,true)
										(OrderFields::status,Status::strExecuted);
					}else {
						sellOrder(OrderFields::status,Status::strActive);
					}
					Document trade;

					double buyerFee = marketCfg->adjustSizeUp(calcFee(tbo->getUser(), amount));
					double sellerFee = marketCfg->adjustTotalUp(calcFee(tso->getUser(), price));

					auto tradeId = lexID::create(tradeCounter);
					tradeCounter += uniform_dist(rnd);
					logInfo({"Trade",dir,price,amount,tradeId});




					trade("_id",tradeId)
						 ("price",price)
						 ("buy",{tbo->getId(),tbo->getUser(),tbd[0], buyerFee})
						 ("sell",{tso->getId(),tso->getUser(),tsd[0], sellerFee})
						 ("size",amount)
						 ("dir",dir)
						 ("time",(std::size_t)now);
					trade.optimize();
					trades.push_back(trade);
				}break;
			case quark::trOrderMove: {
					const quark::TradeResultOderMove &t = dynamic_cast<const quark::TradeResultOderMove &>(r);
					POrder o = t.getOrder();
					Document &changes = o2u[o->getId()];
					if (o->getLimitPrice()) changes(OrderFields::limitPrice,marketCfg->currencyToPrice(o->getLimitPrice()));
					if (o->getTriggerPrice()) changes(OrderFields::stopPrice,marketCfg->currencyToPrice(o->getTriggerPrice()));
				}break;
			case quark::trOrderOk: {
				}break;
			case quark::trOrderCancel: {
					const quark::TradeResultOrderCancel &t = dynamic_cast<const quark::TradeResultOrderCancel &>(r);
					Document &o = o2u[t.getOrder()->getId()];
					o(OrderFields::status,"canceled")
					 (OrderFields::finished,true)
					(OrderFields::error, Object("code",t.getCode()));
				}break;
			case quark::trOrderTrigger: {
					const quark::TradeResultOrderTrigger &t = dynamic_cast<const quark::TradeResultOrderTrigger &>(r);
					Document &o = o2u[t.getOrder()->getId()];
					OrderType::Type ty = t.getOrder()->getType();
					StrViewA st = OrderType::str[ty];
					o("type", st);
				}break;
			case quark::trOrderDelayed: {
					const quark::TradeResultOrderDelayStatus &t = dynamic_cast<const quark::TradeResultOrderDelayStatus &>(r);
					Document &o = o2u[t.getOrder()->getId()];
					o(OrderFields::status, Status::strDelayed);
				}break;
			case quark::trOrderNoBudget: {
					const quark::TradeResultOrderNoBudget &t = dynamic_cast<const quark::TradeResultOrderNoBudget& >(r);
					lastPrice = t.getPrice();
					//just mark the document, this is enough, because it fill be found out of matching and revalidated
					o2u[t.getOrder()->getId()];
				}break;

			}
}

void QuarkApp::rejectOrderBudget(Document order, bool update) {

	rejectOrder(order,
			OrderErrorException(order.getRevValue(), OrderErrorException::insufficientFunds,"Insufficient Funds")
			,update);

}

POrder QuarkApp::docOrder2POrder(const Document& order, double marketBuyBudget) {
	POrder po;
	OrderJsonData odata;
	Value v;
	double x;


	odata.id = order["_id"];
	odata.dir = String(order["dir"]);
	odata.type = String(order["type"]);
	x = order[OrderFields::size].getNumber();
	if (x < marketCfg->assetMin)
		throw OrderRangeError(odata.id, OrderRangeError::minOrderSize,
				marketCfg->assetMin);

	if (x > marketCfg->assetMax)
		throw OrderRangeError(odata.id, OrderRangeError::maxOrderSize,
				marketCfg->assetMax);

	odata.size = marketCfg->assetToSize(x);
	if (odata.size == 0) {
		throw OrderRangeError(odata.id, OrderRangeError::minOrderSize,0);
	}
	if ((v = order[OrderFields::limitPrice]).defined()) {
		x = v.getNumber();
		if (x < marketCfg->currencyMin)
			throw OrderRangeError(odata.id, OrderRangeError::minPrice,
					marketCfg->currencyMin);

		if (x > marketCfg->currencyMax)
			throw OrderRangeError(odata.id, OrderRangeError::maxPrice,
					marketCfg->currencyMax);

		odata.limitPrice = marketCfg->priceToCurrency(x);
	} else {
		odata.limitPrice = 0;
	}
	if ((v = order[OrderFields::stopPrice]).defined()) {
		x = v.getNumber();
		if (x < marketCfg->currencyMin)
			throw OrderRangeError(odata.id, OrderRangeError::minPrice,
					marketCfg->currencyMin);

		if (x > marketCfg->currencyMax)
			throw OrderRangeError(odata.id, OrderRangeError::maxPrice,
					marketCfg->currencyMax);

		odata.stopPrice = marketCfg->priceToCurrency(x);
	} else {
		odata.stopPrice = 0;
	}

	Value userBudget = order[OrderFields::budget];
	if (userBudget.defined()) {
		if (marketBuyBudget == 0) marketBuyBudget = userBudget.getNumber();
		else marketBuyBudget = std::min(marketBuyBudget, userBudget.getNumber());
	}
//	double totalBudget = b.marginLong+b.marginShort+b.currency;
	odata.budget = marketCfg->budgetToFixPt(marketBuyBudget);
	odata.trailingDistance = marketCfg->priceToCurrency(
			order[OrderFields::trailingDistance].getNumber());
	odata.domPriority = order[OrderFields::domPriority].getInt();
	odata.queuePriority = order[OrderFields::queuePriority].getInt();
	odata.user = order[OrderFields::user];
	odata.data = {  //[margin_context, budget_limited]
			order[OrderFields::context].getString() == OrderContext::strMargin,
			order[OrderFields::budget].defined()
	};
	po = new Order(odata);
	return po;
}


void QuarkApp::syncWithDb() {

	Value lastTrade = fetchLastTrade(*tradesDb);
	if (lastTrade != nullptr) {
		tradeCounter = 0;
		lastTradeId = lastTrade["_id"];
		tradeCounter = lexID::parse(lastTradeId.getString(),tradeCounter)+1;
		lastPrice = lastTrade["price"].getNumber();
	} else {
		tradeCounter = 1;
		lastPrice = 1;
		lastTradeId = nullptr;
	}

}

bool QuarkApp::blockOnError(ChangesFeed &chfeed) {

	//hakt processing, if there is error object
	Value errorDoc = ordersDb->get("error", CouchDB::flgNullIfMissing);
	if (!errorDoc.isNull()) {
		logError("Error is signaled, engine stopped - please remove error file to continue");
		chfeed.setFilter(waitfordoc).arg("doc","error")
			 .since(ordersDb->getLastSeqNumber())
			  .setTimeout(-1) >> [](ChangedDoc chdoc) {
				return !chdoc.deleted;
		};
		logInfo("Status clear, going on");
		return !chfeed.wasCanceled();
	}
	return true;


}

void QuarkApp::monitorQueue(std::promise<Action> &exitFnStore) {

	bool canceled = false;

	ChangesFeed chfeed = ordersDb->createChangesFeed();
	exitFnStore.set_value( [&] {
		chfeed.cancelWait();
		canceled = true;
	});

	if (!blockOnError(chfeed)) return;


	initialReceiveMarketConfig();

	Semaphore limitQueued(8);


	auto loopBody = [&](ChangedDoc chdoc) {


		limitQueued.lock();

		logDebug({"queue-Dispatch", chdoc.id});

		[chdoc,this,&limitQueued]{
			limitQueued.unlock();
			if (chdoc.id == marketConfigDocName) {
				try {
					//validate market config;
					MarketConfig c(chdoc.doc);
					if (c.rev != marketCfg->rev) {
						logInfo("Market configuration updated (restart)");
						exitCode = true;
						exitApp();
					}
				} catch (std::exception &e) {
					logError( {	"MarketConfig update failed", e.what()});
				}
			} else if (chdoc.id.substr(0,2) == "o.") {
				processOrder(chdoc.doc);
			}
		} >> dispatcher;
		return true;
	};

	logInfo("[Queue] Reading orders ... (can take long time)");

	Query q = ordersDb->createQuery(queueView);
	Result res = q.needUpdateSeq().exec();
	logInfo({"[Queue] Orders loaded", res.size()});
	for (Value v : res) {
		loopBody(v);
		if (canceled) break;
	}

	if (!canceled) {
		logInfo("[Queue] Processing new orders");

		schedulerIntr.push(true);

		[=]{coreState.startPairing();} >> dispatcher;


		chfeed.setFilter(queueFilter).since(res.getUpdateSeq()).setTimeout(-1)
					>> 	loopBody;
	}

	logInfo("[Queue] Quitting");

	limitQueued.lock_n(8);

	return;




}

void QuarkApp::initMoneyService() {

	PCouchDB orders = ordersDb;
	PCouchDB trades = tradesDb;
	PMarketConfig mcfg = marketCfg;


	Value cfg = marketCfg->moneyService;
	StrViewA type = cfg["type"].getString();
	PMoneySrvClient sv;
	if (type == "mockup") {
		Value maxBudget = cfg["maxBudget"];
		Value jlatency = cfg["latency"];
		if (!maxBudget.defined()) throw std::runtime_error("Missing 'maxBudget' in money service definition");
		if (!jlatency.defined()) throw std::runtime_error("Missing 'latency' in money service definition");
		OrderBudget b(maxBudget["asset"].getNumber(),maxBudget["currency"].getNumber()
				,maxBudget["marginLong"].getNumber(),maxBudget["marginShort"].getNumber(), 0 ,0);
		std::size_t latency =jlatency.getUInt();
		sv = new MockupMoneyService(b,*this,latency);
	} else if (type == "singleJsonRPCServer" || type == "jsonrpc"){
		Value addr = cfg["addr"];
		bool logTrafic = cfg["logTrafic"].getBool();
		String firstTradeId ( cfg["firstTradeId"]);
		sv = new MoneyServerClient2(*this,
						 addr.getString(),
						 signature,
						 marketCfg,
						 firstTradeId,
						 logTrafic);
	} else {
		throw std::runtime_error("Unsupported money service");
	}
	moneySrvClient = new MarginTradingSvc(*positionsDb,marketCfg,sv);
	if (moneyService == nullptr) {
		moneyService = new MoneyService(moneySrvClient,marketCfg,dispatcher);
	} else {
		moneyService->setClient(moneySrvClient);
		moneyService->setMarketConfig(marketCfg);
	}
}

void QuarkApp::applyMarketConfig(Value doc) {
	marketCfg = new MarketConfig(doc);
	initMoneyService();
	coreState.setMaxSpread(std::size_t(floor(marketCfg->maxSpreadPct*100)));
}


void QuarkApp::receiveMarketConfig() {
	Value doc = ordersDb->get(marketConfigDocName, CouchDB::flgNullIfMissing);
	if (doc != nullptr) {
		applyMarketConfig(doc);
	} else {
		throw std::runtime_error("No money service available");
	}
}

void QuarkApp::initialReceiveMarketConfig() {
	Value doc = ordersDb->get(marketConfigDocName, CouchDB::flgNullIfMissing);
	if (doc != nullptr) {
		String s (doc["updateUrl"]);
		if (!s.empty()) {
			updateConfigFromUrl(s, doc["updateLastModified"], doc["updateETag"]);
			doc = ordersDb->get(marketConfigDocName, CouchDB::flgNullIfMissing);
		}
		applyMarketConfig(doc);
	} else {
		throw std::runtime_error("No money service available");
	}
}

bool QuarkApp::initDb(Value cfg, String signature) {

	ordersDb = std::make_shared<CouchDB>(initCouchDBConfig(cfg, signature,"-orders"));
	initOrdersDB(*ordersDb);

	tradesDb = std::make_shared<CouchDB>(initCouchDBConfig(cfg, signature,"-trades"));
	initTradesDB(*tradesDb);

	positionsDb = std::make_shared<CouchDB>(initCouchDBConfig(cfg,signature, "-positions"));
	initPositionsDB(*positionsDb);

	return true;
}


class LogErrors: public ILogProviderFactory {
public:
	LogErrors(PCouchDB db):db(db) {
		prevProvider = setLogProvider(this);
		setLogProvider(createLogProvider());

	}
	~LogErrors() {
		setLogProvider(prevProvider);
	}

	class Provider: public ILogProvider {
		public:
			Provider(PCouchDB db, PLogProvider nx):nx(nx),db(db) {}
			virtual void sendLog(LogType type, json::Value message) {
				nx->sendLog(type,message);
				if (type == error || type == warning) {
					do {
						Document doc = db->get("warning", CouchDB::flgCreateNew);
						doc.set("type",type==warning?"warning":"error")
						   .set("desc",message);
						try {
							db->put(doc);
							break;
						} catch (UpdateException &e) {
							// empty
						} catch(...) {
							//giveup
							break;
						}
					} while (true);
				}
			}

		protected:
			PLogProvider nx;
			PCouchDB db;
		};

	virtual PLogProvider createLogProvider() {
		return new Provider(db, prevProvider->createLogProvider());
	}

protected:
	PCouchDB db;
	ILogProviderFactory *prevProvider;
};

bool QuarkApp::start(Value cfg, String signature)

{

	exitCode = false;
	this->signature = signature;

	setUnhandledExceptionHandler([cfg,signature]{

		//Puts error object to database to prevent engine restart
		//and exits through abort()

		Value errdesc;


		try {

			try {
				throw;
			} catch(RuntimeError &e) {
				errdesc = e.getData();
			} catch(std::exception &e) {
				errdesc = e.what();
			} catch(...) {
				errdesc = "Undetermined exception - catch (...)";
			}

			CouchDB db(initCouchDBConfig(cfg,signature, "-orders"));
			Document errdoc;
			errdoc.setID("error");
			errdoc("what",errdesc);
			time_t t;
			time(&t);
			errdoc("time",t);
			db.put(errdoc);
			logError({"FATAL ERROR", errdesc});
		} catch (std::exception &e) {
			logError({"Fatal error (...and database is not available)",errdesc,e.what()});
		}
		abort();

	});

	logInfo("[start] updating database");
	initDb(cfg,signature);

	LogErrors logErrors(ordersDb);

	logInfo("[start] Syncing... (can take long time)");

	syncWithDb();

	logInfo("[start] Starting scheduler");

	scheduler = std::thread([&]{
		try {
			schedulerWorker();
		} catch (...) {
			unhandledException();
		}
	});


	logInfo("[start] Starting queue");

	{
		std::promise<Action> exitW;
		std::future<Action> f_exitW = exitW.get_future();
		//
		changesReader = std::thread([&]{
			try {
				monitorQueue(exitW);
			} catch (...) {
				unhandledException();
			}});

		//wait until the exit function is known
		exitFn = f_exitW.get();
	}


	logInfo("[start] Dispatching");


	watchdog.start(60000,
		[=](unsigned int nonce)  {
			dispatcher << [=]{
				logInfo({"Watchdog test",nonce});
				watchdog.reply(nonce);
			};
			updateConfig();
		},[=]{
			try {
				throw std::runtime_error("Watchdog failure");
			} catch (...) {
				unhandledException();
			}
		}
		);

	try {

		//run dispatcher now
		dispatcher.run();


		logInfo("[start] Exitting queue");

		//this flag sets to true when everything is ready for exit
		bool exitFlag = false;

		//thread runs dispatcher to finish any pending messages created during exit
		std::thread exitTh([&]{
			//we can exit only when exit flag is set to true
			while (exitFlag == false) {
				//otherwise continue in dispatch
				dispatcher.run();
			}
		});

		//send exit to monitor queue
		exitFn();
		//clear exit function (no longer needed)
		exitFn = nullptr;
		//stop scheduler
		schedulerIntr.push(false);
		//join scheduler
		scheduler.join();
		//join monitor thread
		changesReader.join();

		//destroy money service client
		moneySrvClient = nullptr;
		//destroy money service
		moneyService = nullptr;
		//stop watchdog
		watchdog.stop();
		//now everything is ready to exit
		//raise exit flag (through the dispatcher, to ensure, that this is last message)
		[&]{exitFlag = true;} >> dispatcher;
		//stop dispatcher
		dispatcher.quit();
		//join exit thread
		exitTh.join();
	} catch (...) {
		unhandledException();
	}
	logInfo("[start] Quit ");

	return exitCode;
}




void QuarkApp::controlStop(RpcRequest req) {
	[=] {
		try {
			throw std::runtime_error("Stopped on purpose (Control.stop[])");
		} catch (...) {
			unhandledException();
		}
	} >> dispatcher;

	req.setResult(true);
}


void QuarkApp::updateConfig() {

	PMarketConfig cfg = marketCfg;

	if (cfg == nullptr) return;

	String s = cfg->updateUrl;
	//no update url specified, skip
	if (s.empty()) return;

	updateConfigFromUrl(s,cfg->updateLastModified, cfg->updateETag);
}
bool QuarkApp::updateConfigFromUrl(String s, Value lastModified, Value etag) {
	try {

		couchit::HttpClient client;
		client.setTimeout(10000);
		client.open(s,"GET",false);
		Object hdrs;
		if (lastModified.defined()) {
			hdrs("If-Modified-Since", lastModified);
		}
		if (etag.defined()) {
			hdrs("If-None-Match", etag);
		}
		client.setHeaders(hdrs);
		int status = client.send();
		if (status != 304) {
			logInfo({"[remote_config] Downloading new config",s});
			if (status == 200) {

				Value data = Value::parse(client.getResponse());
				auto hash = std::hash<Value>()(data);

				Document settings = ordersDb->get("settings");
				if (settings["updateHash"].getUInt() != hash) {
					Document newSettings;
					newSettings.setBaseObject(data);
					newSettings.setID(settings.getIDValue());
					newSettings.setRev(settings.getRevValue());
					if (!newSettings["updateUrl"].defined()) {
						newSettings.set("updateUrl", s);
					}
					Value headers = client.getHeaders();
					newSettings.set("updateLastModified",headers["Last-Modified"]);
					newSettings.set("updateETag",headers["ETag"]);
					newSettings.set("updateHash", hash);
					MarketConfig validate(newSettings);
					ordersDb->put(newSettings);
					return true;
				} else {
					logInfo({"[remote_config] No change detected",s});
					return false;
				}
			} else {
				logInfo({"[remote_config] Download failed", status});
				return false;
			}
		}
		return true;
	} catch (std::exception &e) {
		logError({"[remote_config] Error reading config", s, e.what()});
		return false;
	}

}

void QuarkApp::resync(ITradeStream& target, const Value fromTrade, const Value toTrade) {
	quark::resync(*ordersDb,*tradesDb,target,fromTrade,toTrade,*marketCfg);
}

bool QuarkApp::cancelAllOrders(const json::Array& users) {
	do {
		couchit::Query q = ordersDb->createQuery(userActiveOrders);
		if (users.empty()) return false;
		q.keys(users);

		couchit::Result res = q.exec();
		if (res.empty()) {
			return true;
		}

		couchit::Changeset chs = ordersDb->createChangeset();
		for (couchit::Row rw : res) {

			couchit::Document doc(rw.doc);
			doc(OrderFields::cancelReq,true);
			chs.update(doc);

		}
		try {
			chs.commit();
		} catch (couchit::UpdateException &e) {
			//nothing
		}
	} while (true);
}

Dispatcher& QuarkApp::getDispatcher() {
	return dispatcher;
}

void QuarkApp::controlDumpState(RpcRequest req) {
	req.setResult(coreState.toJson());
}
void QuarkApp::controlDumpBlocked(RpcRequest req) {
	req.setResult(moneyService->toJson());
}

void QuarkApp::createNextOrder(Value originOrder, Value nextOrder) {

	if (nextOrder.type() == json::array) {
		for (Value x: nextOrder) {
			createNextOrder(originOrder, x);
		}
	} else if (nextOrder.type() == json::object) {
		Value newId = nextOrder[NextOrderFields::orderId];
		Value target =  nextOrder[NextOrderFields::target];
		Value stoploss =  nextOrder[NextOrderFields::stoploss];
		Value size =  nextOrder[NextOrderFields::size];

		if (size.getInt() < 0) {
			logWarn({"Failed to create nextOrder, invalid size", originOrder[OrderFields::orderId]});
			return;
		}

		if (!newId.defined() || newId.getString().substr(0,2) != "o.") {
			newId = ordersDb->genUIDValue("o.");
		}


		Value dir = originOrder[OrderFields::dir];
		Value context = originOrder[OrderFields::context];
		Value user = originOrder[OrderFields::user];
		Value data = nextOrder[OrderFields::data];
		Value trailing = nextOrder[OrderFields::trailingDistance];
		Value absolute = nextOrder[NextOrderFields::absolute];
		Value next = nextOrder[OrderFields::nextOrder];

		Value strbuy = OrderDir::str[OrderDir::buy];
		Value strsell = OrderDir::str[OrderDir::sell];
		Value limit;
		Value stop;
		double tg = target.getNumber();
		double sl = stoploss.getNumber();

		if (absolute.getBool()) {

			if (dir==strbuy) dir = strsell; else dir = strbuy;
			if (tg > 0) limit = target;
			if (sl > 0) stop = stoploss;

		} else if (dir == strbuy ) {

			dir = strsell;
			if (tg > 0) limit = lastPrice + tg;
			if (sl > 0) stop = lastPrice - sl;

		} else if (dir == strsell ) {

			dir = strbuy;
			if (tg > 0) limit = lastPrice - tg;
			if (sl > 0)	stop = lastPrice + tg;
		} else {
			//unreachable code
			return;
		}

		Value orderType;
		if (stop.defined() && limit.defined()) orderType = OrderType::str[OrderType::oco_limitstop];
		else if (stop.defined()) orderType = OrderType::str[OrderType::stop];
		else if (limit.defined()) orderType = OrderType::str[OrderType::limit];
		else {
			logWarn({"Failed to create nextOrder, need either stoploss or target or both", originOrder[OrderFields::orderId]});
			return;
		}

		time_t now;
		time(&now);

		Document newOrder;
		newOrder.set(OrderFields::orderId, newId)
				(OrderFields::context, context)
				(OrderFields::type, orderType)
				(OrderFields::dir, dir)
				(OrderFields::user, user)
				(OrderFields::size, size)
				(OrderFields::origSize, size)
				(OrderFields::stopPrice, stop)
				(OrderFields::limitPrice, limit)
				(OrderFields::trailingDistance, trailing)
				(OrderFields::data, data)
				(OrderFields::nextOrder, next)
				(OrderFields::status, Status::strValidating)
				(OrderFields::prevOrder, originOrder[OrderFields::orderId])
				(OrderFields::timeCreated, std::size_t(now));


		newOrder.enableTimestamp();

		try {
			ordersDb->put(newOrder);
		} catch (std::exception &e) {
			logError({"Failed to create next order", e.what()});
		}







	}




}

void QuarkApp::schedulerCycle(time_t now) {


	Query q = ordersDb->createQuery(schedulerView);
	q.range(0,now,0);
	q.includeDocs();
	Result res = q.exec();
	Semaphore cntr(1);
	for (Row rw : res) {

		cntr.lock();

		Value order = rw.doc;
		if (order[OrderFields::expireAction].getString() == "market") {
			[&cntr,order,this]{
				Document chorder(order);
				chorder.set(OrderFields::type,OrderType::str[OrderType::market]);
				chorder.set(OrderFields::expired, true);
				chorder.set(OrderFields::status,Status::strExecuted);
				runOrder(chorder,true);
				cntr.unlock();
			} >> dispatcher;
		} else {
			[&cntr,order,this]{
				cancelOrder(order);
				cntr.unlock();
			} >> dispatcher;
		}
	}
	cntr.lock_n(1);
}

void QuarkApp::schedulerWorker() {
	//initial start - scheduler must start after the queue
	bool cont = schedulerIntr.pop();

	while (cont) {


		time_t now;
		time(&now);

		Query q = ordersDb->createQuery(schedulerView);
		q.limit(1);
		Result res = q.exec();
		std::size_t delay = 3600;
		for (Row r: res) {
			time_t nx = r.key.getInt();
			if (nx < now) {
				schedulerCycle(now);
				delay = 0;
			} else {
				delay = nx - now;
			}
		}
		//always wait 1 second to prevent load when many reschedulering
		std::this_thread::sleep_for(std::chrono::seconds(1));
		//wait specified count of seconds or until the reschedule request arrives
		schedulerIntr.pump_for(std::chrono::seconds(delay),[&](bool r){
			cont = r;
		});
		//collect all requests (only if cont true is signaled)
		while (cont && schedulerIntr.try_pump([&](bool r){
			cont = r;
		})) {}


	}

}

void QuarkApp::syncTS(Value cfg, String signature) {
	logInfo("[start] updating database");
	this->signature = signature;

	std::exception_ptr storedException;

	setUnhandledExceptionHandler([&]{
		storedException = std::current_exception();
	});

	initDb(cfg,signature);

	initialReceiveMarketConfig();



	moneySrvClient->resync();
	MTCounter lk(1);

	moneySrvClient->allocBudget("dummy",OrderBudget(),[&](IMoneySrvClient::AllocResult){lk.dec();});
	lk.zeroWait();
	moneyService = nullptr;
	moneySrvClient = nullptr;


	if (storedException != nullptr) {
		 std::rethrow_exception(storedException);
	}
}

void QuarkApp::rememberFee(const Value& user, double fee) {
	std::unique_lock<std::mutex> _(feeMapLock);
	feeMap[user] = fee;
}

double QuarkApp::calcFee(const Value &user, double val)  {
	std::unique_lock<std::mutex> _(feeMapLock);
	auto f = feeMap.find(user);
	if (f != feeMap.end()) {
		return f->second * val; //(add half of fee percent to achieve rounding up)
	} else {
		logWarn({"No fee recorded for user", user});
		return 0;
	}

}

} /* namespace quark */

