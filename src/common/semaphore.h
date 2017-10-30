#pragma once
#include <condition_variable>
#include <mutex>


///Standard semaphoe
class Semaphore {
	typedef std::unique_lock<std::mutex> Sync;
public:

	///Initialize semaphore for 1 - works as lock
	Semaphore(){counter = 1;}
	///Initialize semaphore and set the counter
	Semaphore(unsigned int c):counter(c) {}


	///decrement counter - blocks when counter iz zero
	void lock() {
		Sync _(lk);
		cond.wait(_, [this]{return counter > 0;});
		counter--;
	}

	///increment counter - possible release a next thread
	void unlock() {
		Sync _(lk);
		counter++;
		if (counter == 1) {
			cond.notify_one();
		}
	}

	///try lock
	bool try_lock() {
		Sync _(lk);
		if (counter == 0) return false;
		counter--;
		return true;
	}

	///lock n-times
	void lock_n(unsigned int count) {
		while (count) {
			lock();
			count--;
		}
	}


protected:
	std::mutex lk;
	std::condition_variable cond;
	unsigned int counter;
};

template<typename T>
class SemaphoreGuard {
public:
	SemaphoreGuard(T &x):x(&x) {
		this->x->lock();
	}
	SemaphoreGuard(const SemaphoreGuard &o):x(o.x) {
		if (x) x->lock();
	}
	SemaphoreGuard(SemaphoreGuard &&o):x(o.x) {
		o.x = nullptr;
	}
	~SemaphoreGuard() {
		if (x) x->unlock();
	}

protected:
	T *x;

};
