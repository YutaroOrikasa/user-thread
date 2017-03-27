// notice: API is not stable!

#ifndef USER_THREAD_HPP_
#define USER_THREAD_HPP_

#include <cstdio>
#include <malloc.h>
#include <list>
#include <queue>


#include "mysetjmp.h"
#include "user-thread-internal.hpp"



namespace orks {
namespace userthread {
using namespace detail;

class Thread {
	ThreadData* thread_data;

public:

	explicit Thread(ThreadData& td) :
			thread_data(&td) {
	}

	bool running();

};

class WorkerManager {
	WorkQueue work_queue;
	std::list<Worker> workers;

	std::mutex mutex_for_threads;
	std::list<ThreadData> threads;

public:
	explicit WorkerManager(unsigned int number_of_worker) :
			work_queue(number_of_worker) {

		for (unsigned int i = 0; i < number_of_worker; ++i) {
			workers.emplace_back(work_queue);
		}
	}

	/**
	 * This function blocks until all user threads finish.
	 */
	void start_main_thread(void (*func)(void* arg), void* arg) {
		threads.emplace_back(func, arg, std::make_unique<char[]>(stack_size));
		auto& main_thread = threads.back();

		work_queue.push(main_thread);

		for (auto& worker : workers) {
			worker.wait();
		}

	}

	Thread start_thread(void (*func)(void* arg), void* arg) {
		ThreadData* thread_data;
		{
			auto lk = util::make_unique_lock(mutex_for_threads);
			threads.emplace_back(func, arg,
					std::make_unique<char[]>(stack_size));
			thread_data = &threads.back();
		}

		work_queue.push(*thread_data);

		return Thread(*thread_data);

	}

	/**
	 * user threadがこの関数を呼び出すと、呼び出したuser threadは一時停止し、他のuser threadが動く。
	 * この関数を呼び出したuser threadはスケジューラーによって自動的に再開される。
	 */
	void scheduling_yield() {
		get_worker_of_this_native_thread().schedule_thread();
	}
};


void yield_thread();
Thread start_thread(void (*func)(void* arg), void* arg);

}
}


namespace {
// WorkQueue work_queue { 1 };



}


#endif /* USER_THREAD_HPP_ */
