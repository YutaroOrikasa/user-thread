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

	void start(void* arg);

public:

	explicit Thread(ThreadData& td) :
			thread_data(&td) {
	}

	bool running();
	void continue_();

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
};

void yield_thread();
Thread start_thread(void (*func)(void* arg), void* arg);

}
}


namespace {
// WorkQueue work_queue { 1 };



}
//
//void yield_thread() {
//	if (current_thread == nullptr) {
//		throw std::logic_error("bad operation: yield worker thread");
//	}
//
//	printf("y: %p\n", current_thread);
//
//	current_thread->state = ThreadState::stop;
//
//	if (mysetjmp(current_thread->env)) {
//		printf("jump back!\n");
//		return;
//	}
//	mylongjmp(worker_thread_context);
//}

#endif /* USER_THREAD_HPP_ */
