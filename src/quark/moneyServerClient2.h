#pragma once


#include "imoneysrvclient.h"

namespace quark {


using namespace json;


class MoneyServerClient2: public IMoneySrvClient {
public:


	MoneyServerClient2(PMoneySvcSupport support, String addr, String signature, String asset, String currency);
	~MoneyServerClient2();


	virtual void adjustBudget(json::Value user, OrderBudget &budget) override;
	virtual bool allocBudget(json::Value user, OrderBudget total, Callback callback) override;
	virtual void reportTrade(Value prevTrade, const TradeData &data) override;
	virtual void reportBalanceChange(const BalanceChange &data) override;
	virtual void commitTrade(Value tradeId) override;



protected:

	///dispatcher - it is used to perform connects and post commands from replyes and errors
	PMoneySvcSupport support;

	///connect addr
	const String addr;
	///signature of the client
	const String signature;

	const String asset;

	const String currency;



public:

protected:


};

} /* namespace quark */


