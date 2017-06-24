#include "mockupmoneyserver.h"

#include "error.h"

#include "logfile.h"

namespace quark {

void MockupMoneyService::start() {
	if (workerThread == nullptr)
		workerThread = std::unique_ptr<std::thread>(new std::thread([=]{
			try {
				worker();
			} catch (...) {
				unhandledException();
			}
		}));
}

bool MockupMoneyService::allocBudget(json::Value user, OrderBudget total, Callback callback) {
	std::lock_guard<std::mutex> _(queueLock);
	if (workerThread == nullptr) {
		start();
	}
	queue.push(QueueItem(user,total,callback));
	runBackend.notify_all();
	return false;
}

void MockupMoneyService::stop() {
	if (workerThread != nullptr) {
		finish = true;
		runBackend.notify_all();
		workerThread->join();
	}
}

void MockupMoneyService::worker() {
	while (!finish) {

		std::queue<QueueItem> cpyq;
		{
			std::unique_lock<std::mutex> _(queueLock);
			runBackend.wait(_, [&]{return !queue.empty()||finish;});
			logInfo({"Moneyserver processing batch", queue.size()});
			std::swap(cpyq, queue);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(serverLatency));

		while (!cpyq.empty()) {
			QueueItem itm = cpyq.front();
			cpyq.pop();

			bool res = allocBudget(itm.user,itm.budget);
			if (itm.callBack) itm.callBack(res);
		}
	}
}

void MockupMoneyService::commitTrade(Value tradeId) {
	logInfo({"Commit trade", tradeId});
}

bool MockupMoneyService::allocBudget(json::Value user, const OrderBudget& b) {
	bool allowed = !(b.above(maxBudget));
	logInfo({"Moneyserver-AllocBudget", user, b.toJson(), allowed});
	return allowed;
}


Value MockupMoneyService::reportTrade(Value prevTrade, const TradeData &data) {

	logInfo({"MoneyServer-Trade",data.id,data.price,data.size,OrderDir::str[data.dir],data.timestamp});
	return data.id;

}

bool MockupMoneyService::reportBalanceChange(const BalanceChange &data) {

	logInfo({"MoneyServer-BalChange", data.trade, data.user, OrderContext::str[data.context], data.assetChange, data.currencyChange, data.fee});
	return true;
}

}

void quark::MockupMoneyService::adjustBudget(json::Value , OrderBudget& ) {}
