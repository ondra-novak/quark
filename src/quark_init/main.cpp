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

static String marketName = "";
static String quarkCfgPath = "/etc/quark/quark.conf";
static String rpcCfgPath = "/etc/quark/rpc.conf";
//static String deamonCfgPath = "/etc/couchdb/default.d/quark.ini";
static String deamonBinPath = "/usr/bin/quark";
static String couchDBAdminUser = "admin";
static String couchDBAdminPwd = "";
static String couchDbServer = "http://localhost:5984/";
static String configUrl = "";
static String configEndpointPrefix;



static void inputLine(StrViewA prompt, String &data) {
	std::cout << prompt << " [" << data << "] :";
	std::string buff;
	std::getline(std::cin, buff);
	if (!buff.empty()) {
		data = StrViewA(buff);
	}
	std::cout <<std::endl;
}


static couchit::Config configureDb(StrViewA suffix) {

	couchit::Config cfg;
	cfg.authInfo.username = couchDBAdminUser;
	cfg.authInfo.password = couchDBAdminPwd;
	cfg.baseUrl = couchDbServer;
	cfg.databaseName = String({marketName,"-",suffix});
	return cfg;

}

static couchit::Config getUserDBCfg() {
	couchit::Config cfg;
	cfg.authInfo.username = couchDBAdminUser;
	cfg.authInfo.password = couchDBAdminPwd;
	cfg.baseUrl = couchDbServer;
	cfg.databaseName = "_users";
	return cfg;

}


static void detectVersion() {
	couchit::Config cfg;
	cfg.authInfo.username = couchDBAdminUser;
	cfg.authInfo.password = couchDBAdminPwd;
	std::cout << "Detecting database" << std::endl;
	cfg.baseUrl = couchDbServer;
	CouchDB db(cfg);
	auto c1 = db.getConnection("/");
	Value v = db.requestGET(c1,nullptr,0);
	String ver(v["version"]);
	std::cout << "Version: " << ver<< std::endl;

	if (ver.substr(0,2) == "2.") {
		//version 2.X.X
		//test clustering
		c1 = db.getConnection("/_cluster_setup");
		Value v= db.requestGET(c1, nullptr,0);
		if (v["state"].getString() != "single_node_enabled") {
			throw std::runtime_error("Database must be configured in single_node mode");
		}
		c1 = db.getConnection("/_membership");
		v= db.requestGET(c1, nullptr,0);
		String nodeName ( v["cluster_nodes"][0]);
		std::cout << "Database name: " << nodeName << std::endl;
		configEndpointPrefix = String({"/_node/", nodeName,"/_config"});

	} else if (ver.substr(0,2) == "1.") {
		configEndpointPrefix = String("/_config");
	} else {
		throw std::runtime_error(String({"Unsupported database version: ", ver}).c_str());
	}

}

static void turnOffAdminParty() {
	couchit::Config cfg;
	cfg.baseUrl = couchDbServer;
	CouchDB master (cfg);
	CouchDB::PConnection conn = master.getConnection("/_config");
	conn->add("admins");
	conn->add(couchDBAdminUser);
	try {
		master.requestPUT(conn,couchDBAdminPwd,nullptr,0);
		std::cout << "User " << couchDBAdminUser << " is now administrator" << std::endl;
	} catch (HttpStatusException &e) {
		if (e.getCode() == 401) {
			std::cout << "Skipping setup, database is already initialized" << std::endl;
		} else {
			throw;
		}
	}
}

Value readConfigFile(const String &cfg) {
	std::ifstream inf(cfg.c_str(), std::ios::in);
	if (!inf)
		throw std::runtime_error(String({"Failed to open file ",cfg}).c_str());

	try {
		return Value::fromStream(inf);
	} catch (std::exception &e) {
		throw std::runtime_error(String({"Failed to open read file ",cfg, " ", e.what()}).c_str());
	}
}

void createUser(CouchDB &db, String username, String password, String role) {
	std::cout << "Creatring user: " << username << std::endl;
	try {
		Document user;
	    user.setID(String({"org.couchdb.user:",username}));
	    user("name", username)
	    	("type", "user")
	    	("roles", Value(json::array,{role}))
	    	("password",password);
	    db.put(user);
	} catch (UpdateException &e) {
		std::cout << "Already exists!" << std::endl;
	}
}



static void createDatabase (CouchDB &db) {
	std::cout << "Creating database: " << db.getCurrentDB() << std::endl;
	try {
		db.createDatabase(1,1);
	} catch (HttpStatusException &e) {
		if (e.getCode() == 412) {
			std::cout << "Already exists!" << std::endl;
		} else {
			throw;
		}
	}
	std::cout << "Updating security information for the database" << std::endl;
	Value security = Object("admins",
			Object("names", json::array)("roles", Value(json::array, {
					"quark_daemon" })))("members",
			Object("names", json::array)("roles", Value(json::array, {
					"quark_rpc" })));
	CouchDB::PConnection conn = db.getConnection("_security");
	db.requestPUT(conn,security,nullptr,0);

}


static Value getConfig(CouchDB &db, StrViewA path) {
	CouchDB::PConnection conn = db.getConnection(configEndpointPrefix);
	conn->add(path);
	return db.requestGET(conn,nullptr,0);
}

static Value setConfig(CouchDB &db, StrViewA path, StrViewA key, Value data) {
	CouchDB::PConnection conn = db.getConnection(configEndpointPrefix);
	conn->add(path);
	conn->add(key);
	return db.requestPUT(conn,data,nullptr,0);
}

static void setupDaemonProcess(CouchDB &master) {
	String daemon_name({"quark-",marketName});
	String cmdLine({"\"",deamonBinPath,"\" \"", marketName,"\" \"", quarkCfgPath,"\""});
	setConfig(master, "os_daemons", daemon_name, cmdLine);

}

static bool testFile(const String &str) {
	std::ifstream f(str.c_str(),std::ios::in);
	bool status = !f;
	if (status) {
		std::cout << "File not exists: " << str << std::endl;
		std::cin.sync();
		if (std::cin.eof())
			throw std::runtime_error("EOF in input");
	}

	return status;
}

static void getSettingsFromUrl(Document &settings, String url) {
	HttpClient client;
	client.open(url,"GET",false);
	int status = client.send();
	if (status != 200) {
		throw HttpStatusException(url, status, client.getStatusMessage());
	} else {
		Value cfg = Value::parse(client.getResponse());
		for (Value x: cfg) {
			settings.set(x);
		}
	}
}

static void getSettingsFromFile(Document &settings, String fname) {
	std::ifstream in(fname.c_str(),std::ios::in);
	if (!in) throw std::runtime_error(String({"Failed to open:", fname}).c_str());
	Value cfg = Value::fromStream(in);
	for (Value x: cfg) {
		settings.set(x);
	}
}

int main(int argc, char **argv) {
	try {

		while (marketName.empty())  {
			inputLine("Enter new matching identification", marketName);
		}
		while (configUrl.empty() || (configUrl.substr(0,7) != "http://" && testFile(configUrl) == false))  {
			inputLine("Enter path or URL to market configuration (URL must start with http://) :", configUrl);
		}
		inputLine("Enter URL of the database", couchDbServer);
		inputLine("Enter admin's username", couchDBAdminUser);
		inputLine("Enter admin's password", couchDBAdminPwd);
		do inputLine("Check or change path to Quark daemon's configuration", quarkCfgPath); while (testFile(quarkCfgPath));
		do inputLine("Check or change path to Quark RPC's configuration", rpcCfgPath); while (testFile(rpcCfgPath));
		do inputLine("Check or change path to Quark's binary file", deamonBinPath); while (testFile(deamonBinPath));

		String ack = "y";
		inputLine("Are these informations correct?",ack);
		if (ack != "y" && ack != "Y" && ack != "yes" && ack !="YES" && ack !="Yes")
			return 1;

		turnOffAdminParty();
		detectVersion();
		CouchDB ordersDB(configureDb("orders"));
		CouchDB positionsDB(configureDb("positions"));
		CouchDB tradesDB(configureDb("trades"));
		CouchDB userDB(getUserDBCfg());


		Value daemonCfg = readConfigFile(quarkCfgPath);
		Value rpcCfg = readConfigFile(rpcCfgPath);
		createUser(userDB,String(daemonCfg["username"]), String(daemonCfg["password"]),"quark_daemon");
		createUser(userDB,String(rpcCfg["username"]), String(rpcCfg["password"]),"quark_rpc");
		createDatabase(ordersDB);
		createDatabase(positionsDB);
		createDatabase(tradesDB);
		std::cout<<"Updating market settings" << std::endl;
		Document settings = ordersDB.get("settings",CouchDB::flgCreateNew);
		if (configUrl.substr(0,7) == "http://") {
			getSettingsFromUrl(settings, configUrl);
		} else {
			getSettingsFromFile(settings, configUrl);
		}
		ordersDB.put(settings);
		setConfig(userDB,"compactions","_default","[{db_fragmentation, \"60%\"}, {view_fragmentation, \"60%\"}]");
		std::cout<<"Starting daemon" << std::endl;
		setupDaemonProcess(userDB);
		std::cout<<"Setup complete." << std::endl;
		return 0;
	} catch (std::exception &e) {
		std::cout << "Error: " << e.what();
		return 1;
	}


}

#if 0




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
			<< "  \"daemon_bin\":\"/usr/bin/quark\"," << std::endl
			<< "  \"daemon_cfg\":\"/etc/quark/quark.conf\"," << std::endl
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

void createDB(CouchDB& db, const StrViewA & prefix, const StrViewA & sign, const StrViewA &suffix) {
	db.setCurrentDB( { prefix, sign, "-", suffix });
	std::cout << "Creatring database: " << db.getCurrentDB() << std::endl;
	db.createDatabase();
	std::cout << "Updating security information for the database" << std::endl;
	Value security = Object("admins",
			Object("names", json::array)("roles", Value(json::array, {
					"quark_daemon" })))("members",
			Object("names", json::array)("roles", Value(json::array, {
					"quark_rpc" })));
	CouchDB::PConnection conn = db.getConnection("_security");
	db.requestPUT(conn,security,nullptr,0);
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
	std::cout << "Setting user for daemon: " << jcfg["daemon_login"] << std::endl;
	createUser(db,String(jcfg["daemon_login"]),String(jcfg["daemon_password"]),"quark_daemon");
	std::cout << "Setting user for rpc: " << jcfg["rpc_login"] << std::endl;
	createUser(db,String(jcfg["rpc_login"]),String(jcfg["rpc_password"]),"quark_rpc");

	createDB(db,prefix,sign,"orders");

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
	std::cout << "Uploading market parameters."<< std::endl;
	db.put(settings);

	createDB(db,prefix,sign,"trades");
	createDB(db,prefix,sign,"positions");


	String iniFile= {jcfg["couchdb_etc" ].getString(),"/default.d/quark.ini"};

	std::cout << "Registering daemon (" << iniFile << ") - root required" << std::endl;

	try {
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
	} catch (std::exception &e) {
		std::cout << "Failed to register daemon, are you root? : " << e.what() << std::endl;
	}

	std::cout << "To start matching, please restart couchdb: service couchdb restart" << std::endl;

}

#endif

