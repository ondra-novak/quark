#include "mockupmoneyserver.h"

#include "logfile.h"

namespace quark {

void MockupMoneyService::start() {
	if (workerThread == nullptr)
		workerThread = std::unique_ptr<std::thread>(new std::thread([=]{worker();}));
}

void MockupMoneyService::requestBudgetOnServer(json::Value user,
						OrderBudget total, Callback callback) {
	std::lock_guard<std::mutex> _(queueLock);
	if (workerThread == nullptr) {
		start();
	}
	queue.push(QueueItem(user,total,callback));
	runBackend.notify_all();
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

bool MockupMoneyService::allocBudget(json::Value user, const OrderBudget& b) {
	return !(b.above(maxBudget));
}


Value MockupMoneyService::reportTrade(Value prevTrade, Value id,
		double price, double size, OrderDir::Type dir, std::size_t timestamp) {

	logInfo({"MoneyServer-Trade",id,price,size,OrderDir::str[dir],timestamp});

}

bool MockupMoneyService::reportBalanceChange(Value trade, Value user,
		OrderContext::Type context, double assetChange, double currencyChange,
		double fee) {

	logInfo({"MoneyServer-BalChange", trade, user, OrderContext::str[context], assetChange, currencyChange, fee});
}

}
