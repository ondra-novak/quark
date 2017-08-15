/*
 * main.cpp
 *
 *  Created on: Jul 28, 2017
 *      Author: ondra
 */

#include <unistd.h>	//for isatty()
#include <stdio.h>	//for fileno()
#include <iostream>
#include <couchit/minihttp/netio.h>
#include <imtjson/string.h>
#include <imtjson/parser.h>
#include <imtjson/serializer.h>

#include "../quark/asyncrpcclient.h"

using namespace json;

class RpcClient: public quark::RpcClient {
public:
	virtual void onNotify(const Notify &ntf) {
		std::cout<<"NOTIFY: " << ntf.eventName << " " << ntf.data << std::endl;

	}
};


template<typename Fn>
class JsonParser: public json::Parser<Fn> {
public:
	JsonParser(const Fn &source) :json::Parser<Fn>(source) {}

	virtual Value parse() {
		char c = this->rd.nextWs();
		if (c == '{') {
			this->rd.commit(); return parseObject();
		} else {
			return json::Parser<Fn>::parse();
		}
	}

	inline typename Parser<Fn>::StrIdx readField()
	{
		std::size_t start = this->tmpstr.length();
		int c = this->rd.next();
		while (isalnum(c)) {
			this->rd.commit();
			this->tmpstr.push_back((char)c);
			c = this->rd.next();
		}
		return typename Parser<Fn>::StrIdx(start, this->tmpstr.size()-start);

	}


	inline Value parseObject()
	{
		std::size_t tmpArrPos = this->tmpArr.size();
		char c = this->rd.nextWs();
		if (c == '}') {
			this->rd.commit();
			return Value(object);
		}
		typename Parser<Fn>::StrIdx name(0,0);
		bool cont;
		do {
			if (c == '"') {
				this->rd.commit();
				try {
					name = this->readString();
				}
				catch (ParseError &e) {
					e.addContext(Parser<Fn>::getString(name));
					throw;
				}
			} else {
				name = readField();
			}
			try {
				if (this->rd.nextWs() != ':')
					throw ParseError("Expected ':'");
				this->rd.commit();
				Value v = parse();
				this->tmpArr.push_back(Value(Parser<Fn>::getString(name),v));
				Parser<Fn>::freeString(name);
			}
			catch (ParseError &e) {
				e.addContext(Parser<Fn>::getString(name));
				Parser<Fn>::freeString(name);
				throw;
			}
			c = this->rd.nextWs();
			this->rd.commit();
			if (c == '}') {
				cont = false;
			}
			else if (c == ',') {
				cont = true;
				c = this->rd.nextWs();
			}
			else {
				throw ParseError("Expected ',' or '}'");
			}
		} while (cont);
		StringView<Value> data = this->tmpArr;
		Value res(object, data.substr(tmpArrPos));
		this->tmpArr.resize(tmpArrPos);
		return res;
	}

};

int main(int argc, char **argv) {



	if (argc == 1) {
		std::cerr << "Usage: " << argv[0] << " <address>:port" << std::endl;
		return 1;
	}

	maxPrecisionDigits=9;

	String addr = argv[1];

	RpcClient client;


	if (!client.connect(addr)) {
		std::cerr << "Failed to connect: " << addr << std::endl;
		return 2;
	}




	String prompt;

	if (isatty(fileno(stdin))) {
		prompt = {addr,"> "};
	}

	std::string methodName;
	std::cout << prompt << std::flush;

	int c = std::cin.get();

	while (c != EOF) {

		if (!isspace(c)) {

			methodName.clear();

			while (c != '[' && c != '{' && !isspace(c) ) {
				methodName.push_back((char)c);
				c = std::cin.get();
			}
			std::cin.putback(c);

			if (methodName.empty()) {
				std::cerr << "ERROR: requires name of method" << std::endl;
			} else {

				try {
					JsonParser<json::StreamFromStdStream> parser((json::StreamFromStdStream(std::cin)));
					Value args = parser.parse();
					String name((StrViewA(methodName)));

					RpcResult res = client(name, args);
					if (res.isError()) {
						std::cerr << "ERROR: " << res.toString() << std::endl;
					} else {
						std::cout << res.toString() << std::endl;
					}
				} catch (std::exception &e) {
					std::cerr << "ERROR: " << e.what() << std::endl;
					std::cout << prompt<< std::flush;
				}
			}
		} else {
			if (c == '\n') {
				std::cout << prompt << std::flush;
			}
		}
		c = std::cin.get();
		if (!client.isConnected()) {
			std::cerr << "Lost connection" << std::endl;
			return 3;
		}
	}

	std::cout << std::endl;


}
