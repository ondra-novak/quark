#pragma once

#include <functional>
#include <mutex>
#include <unordered_map>
#include <condition_variable>

namespace quark {


typedef std::function<void()> StdCallback;

template<typename OrderId, typename MapCont= std::unordered_map<OrderId, StdCallback> >
class PendingOrders {
public:

	typedef std::function<bool(StdCallback)> AsyncFn;

	PendingOrders () {}
	~PendingOrders () {waitToFinishAll();}

	///Runs asynchronous function
	/**
	 * @param orderId specifies orderId for with the function will run
	 * @param fn function to run. It need to have prototype bool(std::function<void()>).
	 * @retval false function returned without processing asynchronously
	 * @retval true operation is now pending
	 */
	template<typename AFn>
	bool async(OrderId orderId, AFn fn);



	///Schedules execution of cbfn once the pending order is finished
	/**
	 * @param orderId specifies orderId for with the function will run
	 * @param cbFn function to execute after the order is finished
	 * @retval true operation is scheduled
	 * @retval false order is not pending.
	 */
	template<typename CbFn>
	bool await(OrderId orderId, CbFn cbFn);


	bool isPending(OrderId orderId) const;



	void waitToFinishAll();

protected:

	mutable std::mutex lock;
	typedef std::unique_lock<std::mutex> Sync;
	MapCont orderMap;



};



template<typename OrderId, typename MapCont>
template<typename AFn>
inline bool quark::PendingOrders<OrderId, MapCont>::async(OrderId orderId, AFn fn) {
	{
		Sync _(lock);
		//try register pending order
		auto ins = orderMap.insert(std::make_pair(orderId, nullptr));
		//if was not registered
		if (!ins.second) {
			//pick current registration
			auto fn2 = ins.first->second;
			//if it hasn't await callback
			if (fn2 == nullptr) {
				//create new await callback
				ins.first->second = [=]{
					//repeat async after await
					async(orderId, fn);
				};
			} else {
				//there is already await function
				//then replace it by new function
				ins.first->second = [=]{
					//first call the function registered erlier
					fn2();
					//call async again
					async(orderId, fn);
				};
			}
			//in all cases, return true because operation is pending
			return true;
		}
	}
	//prepare finish handler
	auto finish = [=]{
			//prepare callback function
			StdCallback cb;
			{
				Sync _(lock);
				//receive callback function
				auto f = orderMap.find(orderId);
				//check, whether iterator is valid
				if (f != orderMap.end()) {
					//pick the callback function
					cb = f->second;
					//erase registration
					orderMap.erase(f);
				}
				//unlock the lock

			}
			//if callback defined, call it now
			if (cb != nullptr) cb();
		};

	//call async function
	bool res = fn(finish);
	//if it reported that operation was not async
	if (!res) {
		//finish manually
		finish();
	}//otherwise, it will be finished by the async function
	//return whether operation is pending
	return res;
}

template<typename OrderId, typename MapCont>
template<typename CbFn>
inline bool quark::PendingOrders<OrderId, MapCont>::await(OrderId orderId, CbFn cbFn) {
	//lock internals
	Sync _(lock);
	//find pending order
	auto f = orderMap.find(orderId);
	//not found, then return false;
	if (f == orderMap.end()) return false;
	//receive the function
	auto fn2 = f->second;
	//if there is no function
	if (fn2 == nullptr) {
		//set the function now
		f->second = cbFn;
	} else {
		//queue the function
		f->second = [=] {
			fn2();
			cbFn();
		};
	}
	//operation is now pending
	return true;
}


}

template<typename OrderId, typename MapCont>
inline bool quark::PendingOrders<OrderId, MapCont>::isPending( OrderId orderId) const {
	Sync _(lock);
	auto f = orderMap.find(orderId);
	return f != orderMap.end();

}

template<typename OrderId, typename MapCont>
inline void quark::PendingOrders<OrderId, MapCont>::waitToFinishAll() {

	Sync _(lock);
	std::condition_variable condVar;
	bool status;

	auto waitFn = [&] {
		status = true;
		condVar.notify_all();
	};

	auto f = orderMap.begin();
	while (f != orderMap.end()) {
		status = false;
		auto fn2 = f->second;
		if (fn2 == nullptr) f->second = waitFn;
		else f->second = [&]{fn2();waitFn();};
		condVar.wait(_, [&]{return status;});
	}
}
