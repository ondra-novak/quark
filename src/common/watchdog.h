#pragma once

class Watchdog {
public:
	Watchdog();
	~Watchdog();

	template<typename WatchFn,typename ErrorFn>
	void start(unsigned int intervalms, WatchFn watchFn, ErrorFn errorFn);
	void reply(unsigned int nonce);
	void stop();
protected:
	std::thread thr;
	bool running;
	std::promise<bool> exitSignal;
	unsigned int lastNonce;
};

inline Watchdog::Watchdog() {
	running = false;
}

inline Watchdog::~Watchdog() {
	if (running) stop();
}

template<typename WatchFn, typename ErrorFn>
inline void Watchdog::start(unsigned int intervalms, WatchFn watchFn, ErrorFn errorFn) {
	lastNonce = 0;
	std::shared_future<bool> ef = exitSignal.get_future();
	thr = std::thread([=]{
		unsigned int nonce = 1;
		watchFn(nonce);
		while (ef.wait_for(std::chrono::milliseconds(intervalms)) ==std::future_status::timeout) {
			if (lastNonce != nonce) errorFn();
			nonce++;
			watchFn(nonce);
		}
	});
	running = true;
}

inline void Watchdog::reply(unsigned int nonce) {
	lastNonce = nonce;
}

inline void Watchdog::stop() {
	exitSignal.set_value(true);
	thr.join();
	running = false;
}
