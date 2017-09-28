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
#include <imtjson/stringview.h>
#include <couchit/couchDB.h>
#include <couchit/changes.h>
#include <couchit/document.h>
#include "../common/config.h"

using quark::initCouchDBConfig;



using namespace json;
using namespace couchit;

template<typename Fn>
std::size_t backupDocuments(CouchDB &db, std::time_t timestamp, String fname, StrViewA timestampField, const Fn &filter) {

	std::size_t count = 0;
	std::fstream out(fname.c_str(),std::ios::out|std::ios::app);
	if (!out) {
		throw std::runtime_error(String({"Failed to open file for write: ", fname}).c_str());
	}
	out.seekp(0,out.end);
	if (out.tellp() != 0) {
		throw std::runtime_error(String({"Failed to open file - already exists: ", fname}).c_str());
	}

	char sep = '[';

	ChangesFeed chfeed = db.createChangesFeed();
	chfeed
		.includeDocs(true)
		.setTimeout(1)
		.setFilter(Filter(String(),Filter::allRevs|Filter::allRevs|Filter::attachments))
		>> [&](const ChangedDoc &doc) {
			Value t = doc.doc[timestampField];
			if (t.type() == json::number &&  t.getUInt() <= timestamp && filter(doc.doc, doc.revisions)) {

				t = t.replace("_rev",Value());
				out << sep << std::endl;
				sep = ',';
				doc.doc.toStream(out);
				count++;
			}
			return true;
		};

	if (sep == '[') out << sep;
	out << ']';
	return count;
}

String getBackupName(StrViewA outdir, StrViewA market, Value timestamp, StrViewA suffix) {

	return String({outdir,"/backup-",timestamp.toString(),"-",market,suffix,".json"});
}

class ProgressReport {
public:
	ProgressReport(std::size_t endPos):begPos(-1),endPos(endPos), lastPos(-1) {}

	void operator()(std::size_t pos) const {
		if (begPos  > pos) begPos = pos;
		if (endPos != begPos) {
			std::size_t curPos = (pos-begPos)*100/(endPos-begPos);
			if (curPos != lastPos) {
				std::cout << "Progress: " << curPos << std::endl;
				lastPos = curPos;
			}
		}
	}

protected:
	mutable std::size_t begPos;
	std::size_t endPos;
	mutable std::size_t lastPos;

};

class PurgeCollect {
public:

	PurgeCollect(CouchDB &db):db(db) {}

	void addView(View w) {
		views.push_back(w);
	}

	void addDocument(const String &docId, const Value &changes) {
		Array revs;
		for(Value x: changes) {
			revs.push_back(x["rev"]);
		}
		documents.set(docId, revs);
		if (documents.size() > 1000) {
			purgeSet.push_back(documents);
			documents.clear();
		}
	}


	void runPurge() {
		if (documents.size() > 0) {
			purgeSet.push_back(documents);
		}

		ProgressReport rep(purgeSet.size());
		for (std::size_t i = 0; i< purgeSet.size(); i++) {
			rep(i);
			Value x = purgeSet[i];
			CouchDB::PConnection conn = db.getConnection("_purge");
			conn->http.setTimeout(600000);
			Value res = db.requestPOST(conn, x, nullptr,0);
			for (View &w : views) {
				db.updateView(w,true);
			}
		}
		CouchDB::PConnection conn = db.getConnection("_compact");
		db.requestPOST(conn, Value(), nullptr,0);
	}


protected:
	Object documents;
	std::vector<Value> purgeSet;
	std::vector<View> views;
	CouchDB &db;

};

int main(int argc, char **argv) {

	char **args = argv+1;
	char *name = argv[0];
	int count = argc -1;

	bool autoYes = false;
	bool autoNo = false;
	String outDir (".") ;

	while (count > 0 && args[0][0] == '-') {
		if (args[0][1] == 'y') {autoYes = true;autoNo = false;}
		if (args[0][1] == 'n') {autoNo = true;autoYes = false;}
		if (args[0][1] == 'o' && count > 1) {
			args++;
			count--;
			outDir = args[0];
		}
		args++;
		count--;
	}

	try {
		if (count < 3) {
			std::cerr << "Removes old orders and trades from the database complete. " << std::endl
					 << "This operation can reduce size of the database file." << std::endl
					 << "NOTE: the operation is ireversible. Make sure, that you have " << std::endl
					 << "replicated database before" << std::endl
					 << std::endl
					 << "Usage: " << std::endl
					 << std::endl
					 << name << "[-y][-n][-o dir] <quark's config file>" << "<market-signature>" << "<timestamp>"
					 << std::endl
					 << "-y                      Perform operation without asking" << std::endl
					 << "-n                      Do not purge data, just backup" << std::endl
					 << "-o dir                  Specify output directory for backup" << std::endl
					 << "<quark'config file>     the same config like for the quark daemon" << std::endl
					 << "<market-signature>      name of the market" << std::endl
					 << "<timestamp>             unix timestamp defines end of purgin "
					 << "                          (newer records are untouched) " << std::endl
					 <<  std::endl
					 << "Notes: Only finished orders are removed." <<  std::endl
					 << "       To reduce size of the database after the purge, the database"<< std::endl
					 << "       must be compacted (automaticly or manually)" <<  std::endl;

			return 1;
		}

		String configPath = args[0];
		String market = args[1];
		Value timestamp;
		Value config;
		try {
			timestamp = Value::fromString(args[2]);
			if (timestamp.type() != json::number || (timestamp.flags() & json::numberUnsignedInteger) == 0) {
				std::cerr << "timestamp must be unsigned integer number" << std::endl;
				return 3;
			}
		} catch (...) {
			std::cerr << "Timestamp must be number" << std::endl;
			return 3;
		}

		try {
			std::ifstream fcfg(configPath.c_str(),std::ios::in);
			config = Value::fromStream(fcfg);

		} catch (std::exception &e) {
			std::cerr << "Failed to parse config: " << e.what() << std::endl;
			return 4;
		}

		const char *ordersSuffix = "-orders";
		const char *tradesSuffix = "-trades";

		StrViewA strTradesSfx(tradesSuffix);
		StrViewA strOrderSfx(ordersSuffix);

		CouchDB ordersDB(initCouchDBConfig(config, market, "-orders"));
		CouchDB tradesDB(initCouchDBConfig(config, market, "-trades"));


		PurgeCollect ordersColl(ordersDB);
		PurgeCollect tradesColl(tradesDB);
		ordersColl.addView(View("_design/index/_view/queue"));
		tradesColl.addView(View("_design/trades/_view/chart"));

		std::cout << "Starting backup" << std::endl;
		{
			ProgressReport prep(timestamp.getUInt());
			String fname1 = getBackupName(outDir, market, timestamp, strOrderSfx);
			std::size_t sz1 = backupDocuments(ordersDB, timestamp.getUInt(), fname1,"timeModified",
					[&](const Value &doc, const Value &changes) {
						bool ok = doc["finished"].getBool() || doc["_deleted"].getBool();
						if (ok) ordersColl.addDocument(String(doc["_id"]), changes);
						prep(doc["timeModified"].getUInt());
						return ok;
					});
			std::cout << "Backup orders: " << fname1 << ", count: " << sz1 << std::endl;
		}

		{
			ProgressReport prep(timestamp.getUInt());
			String fname2 = getBackupName(outDir, market, timestamp, strTradesSfx);
			std::size_t sz2 = backupDocuments(tradesDB, timestamp.getUInt(),fname2,"time",
					[&](const Value &doc, const Value &changes) {
						tradesColl.addDocument(String(doc["_id"]), changes);
						prep(doc["time"].getUInt());
						return true;
				});
			std::cout << "Backup trades: " << fname2 << ", count: " << sz2 << std::endl;
		}

		if (!autoNo) {
			bool decision = false;
			if (!autoYes) {

				std::cerr << "Really delete all old records? Operation is irreversible (y/n):";
				std::cin.sync();
				int c = tolower(std::cin.get());
				while (c != EOF && c != 'y' && c != 'n') {
					std::cerr << "Please enter y(yes) or n(no):";
					std::cin.sync();
					c = tolower(std::cin.get());
				}

				if (c == 'y') {
					decision = true;
				}
			} else {
				decision = true;
			}
			if (decision) {
					std::cout << "Erasing orders" << std::endl;
					ordersColl.runPurge();
					std::cout << std::endl<< "Erasing trades" << std::endl;
					tradesColl.runPurge();
					std::cout << std::endl<< "Done." << std::endl;

			}


		}


	} catch (std::exception &e) {
		std::cerr << "Stopped because an error:" << e.what() << std::endl;
		return 2;

	}
}



