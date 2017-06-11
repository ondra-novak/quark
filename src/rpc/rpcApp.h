#pragma once
#include <couchit/couchDB.h>
#include <imtjson/value.h>
#include <imtjson/rpc.h>
#include <iosfwd>

namespace quark {

using namespace json;
using namespace couchit;

class RpcApp {
public:
	RpcApp(Value svctable):svctable(svctable) {}


	void run(std::istream &input, std::ostream &out);

protected:

	typedef std::unique_ptr<CouchDB> PCouchDB;

	PCouchDB ordersDb;
	PCouchDB tradesDb;
	Value svctable;

	RpcServer rpcServer;
};

}
