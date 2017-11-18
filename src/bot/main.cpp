/*
 * main.cpp
 *
 *  Created on: Jul 28, 2017
 *      Author: ondra
 */

#include <unistd.h>	//for isatty()
#include <stdio.h>	//for fileno()
#include <fstream>
#include <random>

#include <couchit/minihttp/httpclient.h>
#include <imtjson/string.h>
#include <imtjson/parser.h>
#include <imtjson/validator.h>
#include "../quark/logfile.h"
#include <jsonrpc_client/asyncrpcclient.h>

using namespace json;

using namespace quark;
using namespace jsonrpc_client;

/*Config
 *
 * {
 * "server": "rpcserver quarka"
 * "source": "vereje api bitfinexu"
 * "maxOrders": maximalni pocet orderu ma trhu (limitnich)
 * "range": rozsah v procentech
 * "delay": prodleva mezi jednotlivymi prikazy
 * "buyer": "ID uzivatele ktery nakupuje"
 * "seller": "ID uzivatele ktery prodava"
 * }
 */


Value readConfig(std::string name) {
	std::ifstream in(name, std::ios::in);
	return Value::fromStream(in);

}

void checkConfig(Value cfg) {
	Value definition = Object
			("unsigned", Value(json::array,{{">=",0}}))
			("positive", Value(json::array,{{">",0}}))
			("uint", Value(json::array,{{">=",0,"integer"}}))
			("natural", Value(json::array,{{">",0,"integer"}}))
			("_root", Object
					("server","string")
					("source","string")
					("maxOrders","natural")
					("range","positive")
					("delay","natural")
					("buyer","string")
					("seller","string")
					("seed","uint")
					("market","string")
					("minSize","positive")
					("maxSize","positive")
					("expiration",{"positive","optional"})
			);

	Validator validator(definition);
	if (!validator.validate(cfg)) {
		throw std::runtime_error(validator.getRejections().toString().c_str());
	}
}

struct BotConfig {

	String source;
	unsigned int maxOrders;
	unsigned int delay;
	unsigned int seed;
	unsigned int orderExpiration;
	double range;
	double minSize;
	double maxSize;
	String seller;
	String buyer;
	String market;

};





bool connectRpc(RpcClient &rpc, const String &addr, const String &market) {

	if (!rpc.isConnected()) {
		if (!rpc.connect(addr)) return false;
		RpcResult res = rpc("init",Object("market",market));
		if (res.isError())
			throw std::runtime_error(res.toString().c_str());
		Value vres(res);
		if (vres["marketStatus"].getString() != "ok") {
			throw std::runtime_error(String(vres["marketStatus"]).c_str());
		}
	}
	return true;


}

struct MState {
	std::size_t sz;
	bool sell;
	double p1,p2;
};

static MState getCountOfCommands(RpcClient &rpc) {
	MState st;
	st.p1 = 1, st.p2 = 1;
	st.sell = false;
	st.sz = 0;
	RpcResult res = rpc("Orderbook.get", {});
	if (res.isError()) {
		logError(res);
		return st;
	} else {
		std::uintptr_t sz = res.size();
		std::uintptr_t sells = 0;
		Value prevCmd = nullptr;
		for (Value v : res) {
			if (v[1].getString() == "sell") {
				sells++;
				if (prevCmd[1].getString() == "buy") {
					st.p1 = v[2].getNumber();
					st.p2 = prevCmd[2].getNumber();
					if (st.p1 > st.p2) std::swap(st.p1, st.p2);
				}
			}
			prevCmd = v;

		}
		st.sz = sz;
		st.sell = sells < sz/2;
		return st;
	}
}

double getCurrentPrice(String url) {
	try {
		couchit::HttpClient client;
		client.open(url,"GET",false);
		int status = client.send();
		if (status != 200) {
			logError({"Failed to retrieve current price", status});
			return 0;
		}
		Value resp = Value::parse<couchit::InputStream>(client.getResponse());
		return resp[0].getNumber();
	} catch (std::exception &e) {
		logError({"Failed to read price", e.what()});
	}


}

bool checkCommand(StrViewA lastId, RpcClient &rpc) {
	if (lastId.empty()) return true;
	RpcResult res = rpc("Order.get", {lastId});
	if (res.isError()) {
		logError(res);
		return true;
	} else {
		Value v = res;
		return v["status"].getString() != "validating";
	}

}

bool checkMarketCommand(StrViewA lastId, RpcClient &rpc) {
	if (lastId.empty()) return true;
	RpcResult res = rpc("Order.get", {lastId});
	if (res.isError()) {
		logError(res);
		return true;
	} else {
		Value v = res;
		logDebug(v);
		return v["finished"].getBool();
	}

}

void runBot(RpcClient &rpc, const BotConfig &cfg) {

	String lastId;
	String lastMarketId;


	std::default_random_engine rnd(cfg.seed);
	std::uniform_real_distribution<> randomSize(0, sqrt(sqrt(cfg.maxSize-cfg.minSize)));


	double curPrice = 0;

	std::thread priceReader([&]{
			for (;;) {

				FILE *f = popen(cfg.source.c_str(), "r");
				Value v;
				try {
					if (f) {
						v = Value::fromFile(f);
					} else {
						logError("Cannot read source");
					}
				} catch (std::exception &e) {
					logError({"Cannot read source", e.what()});
				}
				if (pclose(f) == 0) {
					curPrice =v[0].getNumber();
				}


				logDebug({"Current price", curPrice});

				std::this_thread::sleep_for(std::chrono::seconds(10));

			}

	});

	bool alternate = false;
	for(;;) {

		alternate = !alternate;
		std::this_thread::sleep_for(std::chrono::milliseconds(cfg.delay));
		if (curPrice == 0) continue;


		if (!connectRpc(rpc,rpc.getAddr(), cfg.market)) {
			logError("Failed connect RPC");
			continue;
		}

		if (!checkCommand(lastId, rpc)) {
			logError("Market is not running, skipping this cycle...");
			continue;
		}
		MState cmdCount = getCountOfCommands(rpc);
		double rndsize = powf(randomSize(rnd),4)+ cfg.minSize;
		bool marketCommand = cmdCount.sz >= cfg.maxOrders;

		Value orderType;
		Value limitPrice;
		Value direction;
		Value size = rndsize;
		Value user;
		bool dirBuy;
		bool probablyFlashcrash = false;

		if (marketCommand && !lastMarketId.empty()) {
			if (!checkMarketCommand(lastMarketId, rpc)) {
				probablyFlashcrash = true;
			logWarn({"Market order delayed", lastMarketId});
			}
		}


		if (probablyFlashcrash) {
			orderType = "limit";
			dirBuy = alternate;
			if ((cmdCount.p2 - cmdCount.p1)/((cmdCount.p2 + cmdCount.p1)/2) < 0.0001) {
				orderType = "market";
			} else {
				std::uniform_real_distribution<> pp(cmdCount.p1, cmdCount.p2);
				limitPrice = pp(rnd);
			}
		} else if (marketCommand) {

			orderType = "market";
			dirBuy = !cmdCount.sell;
		} else {
			double rhalf = curPrice*cfg.range/200;
			std::normal_distribution<> randomSize(curPrice, rhalf);
			orderType = "limit";
			limitPrice = randomSize(rnd);
			if (limitPrice.getNumber() < curPrice) {
				dirBuy = true;
			} else {
				dirBuy = false;
			}
		}

		if (dirBuy) {
			user = cfg.buyer;
			direction = "buy";
		} else {
			user = cfg.seller;
			direction = "sell";
		}

		Object order;
		order
				("user",user)
				("dir",direction)
				("type",orderType)
				("size",size)
				("limitPrice",limitPrice)
				("context","exchange");
		if (cfg.orderExpiration) {
			time_t t;
			time(&t);
			t += cfg.orderExpiration;
			order("expireTime", t);
		}

		RpcResult r = rpc("Order.create", order);
		if (r.isError()) {
			logError({"Error creating request", order, Value(r)});
		} else {
			lastId = String(Value(r)[0]);
			logInfo({"Order created", order, lastId});
			if (marketCommand && !probablyFlashcrash)
					lastMarketId = lastId;
		}




	}


}

int main(int argc, char **argv) {

	if (argc != 2) {
		std::cerr << "Need argument: path to config" << std::endl;
		return 1;
	}
	Value cfg;
	try {
		cfg = readConfig(argv[1]);
		checkConfig(cfg);
	} catch (std::exception &e) {
		std::cerr << "Config validation failed: " << e.what();
		return 2;
	}

	try {

		RpcClient rpc;
		if (!connectRpc(rpc,String(cfg["server"]),String(cfg["market"]))) {
			std::cerr << "Failed to connect RPC" << std::endl;
		}



		BotConfig bcfg;
		bcfg.buyer = String(cfg["buyer"]);
		bcfg.seller = String(cfg["seller"]);
		bcfg.delay = cfg["delay"].getUInt();
		bcfg.seed = cfg["seed"].getUInt();
		bcfg.maxOrders = cfg["maxOrders"].getUInt();
		bcfg.range = cfg["range"].getNumber();
		bcfg.market = String(cfg["market"]);
		bcfg.minSize = cfg["minSize"].getNumber();
		bcfg.maxSize = cfg["maxSize"].getNumber();
		bcfg.source = String(cfg["source"]);
		bcfg.orderExpiration = cfg["expiration"].defined()?cfg["expiration"].getUInt():bcfg.maxOrders*bcfg.delay;

		runBot(rpc, bcfg);


	} catch (std::exception &e) {
		std::cerr << "Error: " << e.what();
		logError({"Uncaught exception", e.what()});
	}

}
