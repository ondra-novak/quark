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
static String initMode = "daemon";



static void inputLine(StrViewA prompt, String &data) {
	std::cout << prompt << " [" << data << "]:";
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
/*

static Value getConfig(CouchDB &db, StrViewA path) {
	CouchDB::PConnection conn = db.getConnection(configEndpointPrefix);
	conn->add(path);
	return db.requestGET(conn,nullptr,0);
}
*/
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

static void setupDatabase(CouchDB &db) {



	String daemon_name({"quark-",marketName});
	String cmdLine({"\"",deamonBinPath,"\" --initdb \"", marketName,"\" \"", quarkCfgPath,"\""});
	int err = system(cmdLine.c_str());
	if (err!=0) {
		std::cout << "Cannot initialize database, process exited with status: " << err;
		throw std::runtime_error("Init failed");
	}


	Value ddoc = Object("_id","_design/readonly")
			("validate_doc_update",
					"function(doc,old_doc,user) "
					"{"
						"if (user.roles.indexOf(\"quark_rpc\") != -1 || user.roles.indexOf(\"quark_daemon\") != -1) "
							"throw ({forbidden:\"Database is readonly (replicated copy)\"}) "
					"}");

	db.putDesignDocument(ddoc);

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
		settings.set("updateUrl",url);
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

		bool modero;

		do {
			inputLine("Type of instalation: \n\tdaemon - start matching daemon,\n\treadonly - initialize to view replicated data\n", initMode);
		} while (initMode != "daemon" && initMode != "readonly");

		modero = initMode == "readonly";

		while (marketName.empty())  {
			inputLine("Enter new matching identification", marketName);
		}
		if (!modero) {
			while (configUrl.empty() || (configUrl.substr(0,7) != "http://" && testFile(configUrl) == false))  {
				inputLine("Enter path or URL to market configuration (URL must start with http://) :", configUrl);
			}
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
		setConfig(userDB,"compactions","_default","[{db_fragmentation, \"60%\"}, {view_fragmentation, \"60%\"}]");
		if (!modero) {
			std::cout<<"Updating market settings" << std::endl;
			Document settings = ordersDB.get("settings",CouchDB::flgCreateNew);
			if (configUrl.substr(0,7) == "http://") {
				getSettingsFromUrl(settings, configUrl);
			} else {
				getSettingsFromFile(settings, configUrl);
			}
			ordersDB.put(settings);

			std::cout<<"Starting daemon" << std::endl;
			setupDaemonProcess(userDB);
		} else {
			std::cout<<"Initializing database" << std::endl;
			setupDatabase(ordersDB);
		}
		std::cout<<"Setup complete." << std::endl;
		return 0;
	} catch (std::exception &e) {
		std::cout << "Error: " << e.what();
		return 1;
	}


}

