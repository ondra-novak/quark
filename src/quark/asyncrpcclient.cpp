/*
 * asyncrpcclient.cpp
 *
 *  Created on: 23. 7. 2017
 *      Author: ondra
 */




#include "asyncrpcclient.h"

#include <couchit/minihttp/buffered.h>

#include <couchit/minihttp/netio.h>
#include <imtjson/parser.h>
#include <imtjson/serializer.h>
#include <cerrno>



#include "logfile.h"


namespace quark {

using namespace couchit;


RpcClient::RpcClient():AbstractRpcClient(RpcVersion::ver2),connected(false) {


}

void RpcClient::disconnect(bool sync) {
	Sync _(lock);
	if (cancelFn != nullptr) {
		cancelFn();
	}
	if (sync) {
		_.unlock();
		if (workerThr.joinable()) {
			workerThr.join();
		}
	}
}

bool RpcClient::connect(StrViewA addr) {
	Sync _(lock);
	connectLk( addr);
	return connected;
}

void RpcClient::connectLk(StrViewA addr) {

	if (!connected) {

		this->addr = addr;

		PNetworkConection nconn = NetworkConnection::connect(addr,1024);
		if (nconn != nullptr) {

			cancelFn = NetworkConnection::createCancelFunction();
			nconn->setCancelFunction(cancelFn);

			if (workerThr.joinable()) workerThr.join();
			workerThr = std::thread([=]{
				worker(nconn);
			});
			conn = nconn;
			connected = true;

		} else {
			int err = errno;
			logError({"Failed to connect", addr, err});
			conn = nullptr;
			rejectAllPendingCalls();
			return;
		}

		onInit();
	}


}

void RpcClient::sendJSON(const Value& v) {
	if (logTrafic) logInfo({"RPC_send",addr,v});
	OutputStream out(conn);
	BufferedWrite<OutputStream> wr(out);
	v.serialize<BufferedWrite<OutputStream> &>(wr);
	wr('\n');
	wr.flush();
	if (conn->getLastSendError()) {
		logError(
				{ "RpcClient", "Failed to send message", addr,
						conn->getLastSendError() });
		cancelFn();
	} else if (conn->isTimeout()) {
		logError( { "RpcClient", "Timeout while writting message", addr });
		cancelFn();
	}
}

void RpcClient::sendRequest(Value request) {
	if (!connected) {
		rejectAllPendingCalls();
	} else  {

		Value v = request;

		sendJSON(v);
	}
}

RpcClient::~RpcClient() {
	disconnect(true);
}

void RpcClient::worker(PNetworkConection conn) {
	while (conn->waitForInput(-1)) {
		try {
			InputStream in(conn);
			BinaryView b = in(0);
			if (b.length == 0) {
				//connection closed
				break;
			}
			if (isspace(b[0])) {
				//skip whitespace
				in(1);
				continue;
			}
			BufferedRead<InputStream> rd(in);
			Value resp = Value::parse<BufferedRead<InputStream> &>(rd);
			if (logTrafic) logInfo({"RPC_receive",addr,resp});
			ReceiveStatus st = processResponse(resp);
			if (st == notification) {
				onNotify(json::Notify(resp));
			} else if (st == request) {
				RpcRequest req = RpcRequest::create(resp, [&](Value v){
					Sync _(lock);
					sendJSON(v);
				});
				req.setError(RpcServer::errorMethodNotFound, "RPCServer mode is not supported");
			}
		} catch (std::exception &e) {
			logError({"Error reading response", addr, e.what()});
			break;
		}
	}
	Sync _(lock);
	connected = false;
	rejectAllPendingCalls();
	this->conn = nullptr;
}

}
