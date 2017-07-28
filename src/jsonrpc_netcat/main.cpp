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
#include "../quark/asyncrpcclient.h"

using namespace json;

class RpcClient: public quark::RpcClient {
public:
	virtual void onNotify(const Notify &ntf) {
		std::cout<<"NOTIFY: " << ntf.eventName << " " << ntf.data << std::endl;

	}
};


int main(int argc, char **argv) {



	if (argc == 1) {
		std::cerr << "Usage: " << argv[0] << " <address>:port" << std::endl;
		return 1;
	}

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
					Value args = Value::fromStream(std::cin);
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
		}
	}

	std::cout << std::endl;


}
