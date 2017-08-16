#pragma once


#include "asyncrpcclient.h"
#include "imoneysrvclient.h"

namespace quark {


using namespace json;


class MoneyServerClient2: public IMoneySrvClient {
public:


	MoneyServerClient2(PMoneySvcSupport support,
				String addr,
				String signature,
				String asset,
				String currency,
				String firstTradeId,
				bool logTrafic);
	~MoneyServerClient2();


	virtual void adjustBudget(json::Value user, OrderBudget &budget) override;
	virtual bool allocBudget(json::Value user, OrderBudget total, Callback callback) override;
	virtual void reportTrade(Value prevTrade, const TradeData &data) override;




protected:


	class MyClient: public RpcClient, public RefCntObj {
	public:
		MyClient(String addr, MoneyServerClient2 &owner);

		virtual void onInit() override;
		virtual void onNotify(const Notify &ntf) override;

		void close() {closed = true;disconnect(true);}
		bool isClosed() const {return closed;}

	protected:
		MoneyServerClient2 &owner;
		bool closed;


	};

	///dispatcher - it is used to perform connects and post commands from replyes and errors
	PMoneySvcSupport support;

	///connect addr
	const String addr;
	///signature of the client
	const String signature;

	const String asset;

	const String currency;
	const String firstTradeId;

	RefCntPtr<MyClient> client;

	void onInit();
	void onNotify(const Notify &ntf) ;

	bool inited;

	Value lastReportedTrade;

	int retryCounter = 0;
	time_t lastDropTime = 0;

public:

protected:

	void connectIfNeed();

	static void handleError(MyClient *c, StrViewA method, const RpcResult &res);
	template<typename Fn>
	static void callWithRetry(RefCntPtr<MyClient> client, PMoneySvcSupport supp, String methodName, Value params, Fn callback);

	class ResyncStream;
	void reportTrade2(Value prevTrade, const TradeData &data) ;

};

} /* namespace quark */


