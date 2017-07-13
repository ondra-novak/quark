/*
 * main.cpp
 *
 *  Created on: Jul 14, 2017
 *      Author: ondra
 */
#include <unistd.h>
#include <fstream>
#include <stdexcept>

#include <imtjson/value.h>
#include <couchit/couchDB.h>
#include <couchit/document.h>



using namespace json;
using namespace couchit;

void createConfig(std::ostream &out);
void createMarket(Value cfg);


int main(int argc, char **argv) {
	try {
		if (argc == 1) {
			createConfig(std::cout);
		}

		if (argc == 2) {
			StrViewA param = argv[1];
			if (param == "-h" || param == "-?") {
				std::cout << "Usage:" << std::endl
						  << std::endl
						  << argv[0] << " -h             - help page" << std::endl
						  << argv[0] << " <config file>  - creates market defined in the config file" << std::endl
						  << argv[0] << "                - (with no arguments) dump config template"  << std::endl;
				return 1;
			}

			else {

				std::ifstream f(argv[1],std::ios::in);
				if (!f) throw std::runtime_error("Failed to open config");
				Value cfg = Value::fromStream(f);

				createMarket(cfg);

			}
		}
	} catch (std::exception &e) {
		std::cerr << "Error: " << e.what();
	}

}

void createConfig(std::ostream &out) {

	out << "{" << std::endl
			<< "  \"database_url\":\"http://localhost:5984/\"," << std::endl
			<< "  \"prefix\":\"\"," << std::endl
			<< "  \"admin_login\":\"admin\"," << std::endl
			<< "  \"admin_password\":\"iamroot\"," << std::endl
			<< "  \"daemon_login\":\"quark_daemon\"," << std::endl
			<< "  \"daemon_password\":\"quark\"," << std::endl
			<< "  \"rpc_login\":\"quark_rpc\"," << std::endl
			<< "  \"rpc_password\":\"quark\"," << std::endl
			<< "  \"couchdb_etc\":\"/etc/couchdb\"," << std::endl
			<< "  \"daemon_bin\":\"/usr/local/bin/quark\"," << std::endl
			<< "  \"daemon_cfg\":\"/usr/local/etc/quark/quark.conf\"," << std::endl
			<< "  \"marketName\":\"<enter_market_name>\"," << std::endl
			<< "  \"asset\":\"<enter_asset_signature>\"," << std::endl
			<< "  \"currency\":\"<enter_currency_signature>\"," << std::endl
			<< "  \"granuality\":<enter_size_granuality>," << std::endl
			<< "  \"pipSize\":<enter_minimum_price_step>," << std::endl
			<< "  \"maxPrice\":<enter_max_price>," << std::endl
			<< "  \"maxSize\":<enter_max_size>," << std::endl
			<< "  \"minPrice\":<enter_min_price>," << std::endl
			<< "  \"minSize\":<enter_min_size>" << std::endl
			<< "}" << std::endl;

}
void createUser(CouchDB &db, String username, String password, String role) {
	try {
		Document user;
	    user.setID(String({"org.couchdb.user:",username}));
	    user("name", username)
	    	("type", "user")
	    	("roles", Value(json::array,{role}))
	    	("password",password);
	    db.put(user);
	} catch (std::exception &e) {
		std::cout << "INFO: " << e.what() << std::endl;
	}
}

void createMarket(Value jcfg) {
	couchit::Config cfg;
	cfg.authInfo.username = String(jcfg["admin_login"]);
	cfg.authInfo.password= String(jcfg["admin_password"]);
	cfg.baseUrl= String(jcfg["database_url"]);
	String sign (jcfg["marketName"]);
	String prefix ( jcfg["prefix"]);

	CouchDB db(cfg);
	db.setCurrentDB("_users");
	createUser(db,String(jcfg["daemon_login"]),String(jcfg["daemon_password"]),"quark_daemon");
	createUser(db,String(jcfg["rpc_login"]),String(jcfg["rpc_password"]),"quark_rpc");

	db.setCurrentDB({prefix,sign,"-orders"});
	db.createDatabase();
	Value security = Object("admins",
				Object("names",json::array)
					  ("roles",Value(json::array,{"quark_daemon"})))
					("members",Object("names",json::array)
							         ("roles",Value(json::array,{"quark_rpc"})));

	CouchDB::PConnection conn = db.getConnection("_security");
	db.requestPUT(conn,security,nullptr,0);
	Document settings;
	settings.setID("settings");
	settings("granuality",jcfg["granuality"])
	  	  ("maxBudget", jcfg["maxPrice"].getNumber()*jcfg["maxSize"].getNumber())
	  	  ("maxPrice", jcfg["maxPrice"])
	  	  ("maxSize", jcfg["maxSize"])
	  	  ("maxSlippagePct", 2.5)
	  	  ("maxSpreadPct", 2.5)
	  	  ("minPrice", jcfg["minPrice"])
	  	  ("minSize", jcfg["minSize"])
	  	  ("pipSize", jcfg["pipSize"])
	  	  ("currencySign", jcfg["currency"])
	  	  ("assetSign", jcfg["asset"])
	  	  ("moneyService", Object
	  			  ("type", "mockup")
	  			  ("maxBudget", Object
	  					  ("currency", 100000)
	  					  ("asset", 1000)
	  					  ("marginLong", 100000)
	  					  ("marginShort", 100000)
	  			  )
	  			  ("latency", 1000)
	  	 );
	db.put(settings);

	db.setCurrentDB({prefix,sign,"-trades"});
	db.createDatabase();
	conn = db.getConnection("_security");
	db.requestPUT(conn,security,nullptr,0);

	db.setCurrentDB({prefix,sign,"-positions"});
	db.createDatabase();
	conn = db.getConnection("_security");
	db.requestPUT(conn,security,nullptr,0);

	String iniFile= {jcfg["couchdb_etc" ].getString(),"/default.d/quark.ini"};
	if (access(iniFile.c_str(),0) != 0) {
		std::ofstream ini(iniFile.c_str(), std::ios::out);
		if (!ini) throw std::runtime_error("Failed to write ini file");
		ini << "[os_daemons]" << std::endl;
	}
	std::ofstream ini(iniFile.c_str(), std::ios::out|std::ios::app);
	if (!ini) throw std::runtime_error("Failed to write ini file");
	ini << "quark_" << sign << " = "
			<< jcfg["daemon_bin"].getString() << " "
			<< sign << " " << jcfg["daemon_cfg"].getString() << std::endl;

}



