#include <couchit/couchDB.h>
#include <imtjson/stringview.h>
#include <imtjson/value.h>
namespace quark {

couchit::Config initCouchDBConfig(json::Value cfgjson,  StrViewA dbname, json::StrViewA suffix) {
	couchit::Config cfg;
	if (cfgjson["singleDB"].getBool()) {
		suffix = StrViewA();
	}
	cfg.authInfo.username = json::String(cfgjson["username"]);
	cfg.authInfo.password = json::String(cfgjson["password"]);
	cfg.baseUrl = json::String(cfgjson["server"]);
	cfg.databaseName = String({cfgjson["dbprefix"].getString(),dbname,suffix});
	return cfg;
}

}
