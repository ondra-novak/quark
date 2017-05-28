#include <imtjson/parser.h>
#include <imtjson/array.h>
#include "../quark_lib/JSONStream.h"
#include "../quark_lib/orderErrorException.h"
#include "../quark_lib/order.h"
#include "../quark_lib/engineState.h"

static quark::CurrentState curState;
static std::vector<quark::TxItem> txbuffer;



json::Value processTransaction(json::Value id, json::Value time, json::Value e) {


	txbuffer.clear();


	for (json::Value v : e) {
		json::Value cmd (v["cmd"]);
		if (cmd.getString() =="add_order") {

			quark::POrder o = new quark::Order(v);
			quark::TxItem itm;
			itm.action = quark::actionAddOrder;
			itm.order = o;
			itm.orderId = o->getId();
			txbuffer.push_back(itm);

		} else if (cmd.getString() == "update_order") {
			quark::POrder o = new quark::Order(v);
			quark::TxItem itm;
			itm.action = quark::actionUpdateOrder;
			itm.order = o;
			itm.orderId = o->getId();
			txbuffer.push_back(itm);
		} else if (cmd.getString() == "cancel_order") {
			quark::TxItem itm;
			itm.orderId = v["orderId"];
			txbuffer.push_back(itm);
		}
	}

	json::Array outres;

	try {
		curState.matching(id, txbuffer, [&](const quark::ITradeResult &r) {
			switch (r.getType()) {
			case quark::trTrade: {
				json::Object o;
					const quark::TradeResultTrade &t = dynamic_cast<const quark::TradeResultTrade &>(r);
					o("cmd","trade")
					 ("buyCmd", json::Object("orderId",t.getBuyOrder()->getId())
									  ("partial",!t.isFullBuy())
									  ("remain",t.isFullBuy()?0:t.getBuyOrder()->getSize()-t.getSize()))

					 ("sellCmd",json::Object("orderId",t.getSellOrder()->getId())
							  ("partial",!t.isFullSell())
							  ("remain",t.isFullSell()?0:t.getSellOrder()->getSize()-t.getSize()))
					 ("size",t.getSize())
					 ("price",t.getPrice())
					 ("dir",t.getDir() == quark::Order::buy?"buy":"sell");
					 outres.push_back(o);
				}break;
			case quark::trOrderMove: {
					const quark::TradeResultOderMove &t = dynamic_cast<const quark::TradeResultOderMove &>(r);
					json::Object o;
					o("cmd","order_move")
					("orderId",t.getOrder()->getId())
					("limitPrice",t.getOrder()->getLimitPrice())
					("stopPrice",t.getOrder()->getTriggerPrice());
					 outres.push_back(o);
				}break;
			case quark::trOrderOk: {
					const quark::TradeResultOrderOk &t = dynamic_cast<const quark::TradeResultOrderOk &>(r);
					json::Object o;
					o("cmd","order_ok")
					("orderId",t.getOrder()->getId());
					 outres.push_back(o);
				}break;
			case quark::trOrderCancel: {
					const quark::TradeResultOrderCancel &t = dynamic_cast<const quark::TradeResultOrderCancel &>(r);
					json::Object o;
					o("cmd","order_cancel")
					("orderId",t.getOrder()->getId())
					("code",t.getCode())
					("message","");
					 outres.push_back(o);
				}break;
			case quark::trOrderTrigger: {
					const quark::TradeResultOrderTrigger &t = dynamic_cast<const quark::TradeResultOrderTrigger &>(r);
					json::Object o;
					o("cmd","order_trigger")
					("orderId",t.getOrder()->getId());
					 outres.push_back(o);
				}break;
			}
		});

	} catch (quark::OrderErrorException &e) {
		outres.clear();
		outres.push_back(json::Object
				("cmd","order_error")
				("orderId",e.getOrderId())
				("code",e.getCode())
				("message",e.getMessage()));

	}

	return json::Object("id", id)
			("time", time)
			("events", outres);
}

json::Value rollbackTransaction(json::Value id, json::Value tx) {

	return json::Object("id",id)
			("success",false)
			("last",nullptr)
			("error","Not implemented yet");


}




json::Value processCommand(json::Value command) {

	using namespace json;
	using namespace quark;

	Value id = command["id"];
	Value e;
	if ((e = command["events"]).defined()) {

		Value time = command["time"];
		return processTransaction(id, time, e);

	} else if ((e = command["rollback"]).defined()) {

		return rollbackTransaction(id, e);

	} else {

		return Object("id",id)
				("error","request")
				("message","Unknown request");

	}


}



int main(int argc, char **argv) {

	using namespace json;
	using namespace quark;


	JSONStream stream(std::cin, std::cout);

	try {

		while (!stream.isEof()) {
			Value cmd = stream.read();
			try {
				Value res = processCommand(cmd);

				stream.write(res);

			} catch (OrderErrorException &e) {
				Object err;
				err("id", cmd["id"])
				   ("error","order_error")
				   ("order",e.getOrderId())
				   ("code",e.getCode())
				   ("message",e.getMessage());

			} catch (std::exception &e) {
				Object err;
				err("id", cmd["id"])
				   ("error","internal_error")
				   ("message",e.what());

			}
		}



	} catch (std::exception &e) {

		Object err;
		err("error","fatal");
		   ("message",e.what());




	}



}
