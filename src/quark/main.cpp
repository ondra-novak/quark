#include <imtjson/parser.h>
#include "../quark_lib/JSONStream.h"
#include "../quark_lib/orderErrorException.h"





json::Value processTransaction(json::Value id, json::Value time, json::Value e) {


	json::Value result = json::array;


	return json::Object("id", id)
			("time", time)
			("events", result);
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
