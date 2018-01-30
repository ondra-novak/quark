#include "mockupmoneyserver.h"

#include <random>


#include "error.h"

#include "logfile.h"

namespace quark {

void MockupMoneyService::start() {
	workerThread = std::thread([=]{
			try {
				worker();
			} catch (...) {
				unhandledException();
			}
		});
}

bool MockupMoneyService::allocBudget(json::Value user, OrderBudget total, Callback callback) {
	queue.push(
			PQueueItem(
					new QueueItem(user,total,callback,
							std::chrono::steady_clock::now() + std::chrono::milliseconds(serverLatency)
					)));
	return false;
}

void MockupMoneyService::stop() {
	queue.push(nullptr);
	workerThread.join();
	logInfo("Moneyserver-Thread stopped");

}

void MockupMoneyService::worker() {
	std::default_random_engine rnd((std::random_device()()));
	std::uniform_int_distribution<int> rd(0,20);
	PQueueItem x = queue.pop();
	while (x != nullptr) {

		std::this_thread::sleep_until(x->execTime);
		if (rd(rnd) == 0 && x->callBack) {
			logInfo({"Moneyserver-Simulated failure", x->user, x->budget.toJson()});
			x->callBack(allocTryAgain);
		} else {
			bool res = allocBudget(x->user, x->budget);
			if (x->callBack) x->callBack(res?allocOk:allocReject);
		}
		x = queue.pop();
	}
}

/*void MockupMoneyService::commitTrade(Value tradeId) {
	logInfo({"Commit trade", tradeId});
}*/

bool MockupMoneyService::allocBudget(json::Value user, const OrderBudget& b) {
	bool allowed = !(b.above(maxBudget));
	logInfo({"Moneyserver-AllocBudget", user, b.toJson(), allowed});
	support.rememberFee(user, 0.001);
	return allowed;
}


void MockupMoneyService::reportTrade(Value prevTrade, const TradeData &data) {

	logInfo({"MoneyServer-Trade",data.id,data.price,data.size,OrderDir::str[data.dir],data.timestamp});

}

/*void MockupMoneyService::reportBalanceChange(const BalanceChange &data) {

	logInfo({"MoneyServer-BalChange", data.trade, data.user, OrderContext::str[data.context], data.assetChange, data.currencyChange, data.fee});

}*/

}

void quark::MockupMoneyService::adjustBudget(json::Value, OrderBudget&) {
}

void quark::MockupMoneyService::resync() {
	logError("Mockup moneyServer doesn't support resync()");
}
