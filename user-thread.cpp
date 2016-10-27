#include <cstdio>
#include <malloc.h>
#include <list>
#include <queue>
#include <thread>
#include <future>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include "user-thread.hpp"

//class ThreadManager {
//public:
//	Thread start_thread(void (*func)(void* arg), void* arg);
//	void yield_thread(Thread& thread);
//};

namespace {
void execute_thread(ThreadData& thread_data);
}

// internal helper
namespace {
template<typename F>
class scope_exit {
	F f;
	bool execute;
public:
	explicit scope_exit(F disposer) :
			f(disposer), execute(true) {
	}
	scope_exit(scope_exit&& other) :
			f(other.f), execute(other.execute) {
		other.execute = false;
	}
	~scope_exit() {
		if (execute) {
			try {
				f();
			} catch (...) {
			}
		}
	}

};

template<typename F>
auto make_scope_exit(F f) {
	return scope_exit<F>(f);
}

template<class Mutex>
auto make_unique_lock(Mutex& mutex) {
	return std::unique_lock<Mutex>(mutex);
}
}

/*
 * pop時にnullptrが返った場合はqueueがcloseされたことを表す。
 */
class WorkQueue {
	std::recursive_mutex mutex;
	std::queue<ThreadData*> queue;
	std::condition_variable_any cv;
	const int number_of_workers;

	// pop()を実行しているworkerの数
	int number_of_hungry_workers = 0;

	// set to true when first push is occured
	bool is_queue_started = false;

public:

	explicit
	WorkQueue(int number_of_workers) :
			number_of_workers(number_of_workers) {
	}

	// you can not push nullptr
	void push(ThreadData& td) {
		push_impl(&td, false);
	}

	ThreadData* pop() {
		auto lock = make_unique_lock(mutex);
		++number_of_hungry_workers;
		auto finally = make_scope_exit([&]() {
			--number_of_hungry_workers;
		});

		// ignore when queue is not started
		if (is_queue_started) {
			// if all workers are hungry
			if (number_of_hungry_workers == number_of_workers) {
				close();
			}
		}
		cv.wait(lock, [this]() {
			return is_queue_started && !this->queue.empty();
		});
		ThreadData* td = queue.front();
		// queueのcloseを表すnullptrがあった場合はpopせずに残す。
		if (td != nullptr) {
			queue.pop();
		}
		return td;
	}

private:
	void push_impl(ThreadData* td, bool notify_all) {

		auto lock = make_unique_lock(mutex);
		printf("WQ::push %p\n", td);
		is_queue_started = true;
		queue.push(td);

		if (notify_all) {
			cv.notify_all();
		} else {
			cv.notify_one();
		}
	}

	void close() {
		printf("WQ::close\n");
		push_impl(nullptr, true);
	}

	/*
	 * 内部仕様
	 * すべてのworkerがpopしようとしたことが検出されたらwork queueをcloseする。
	 */

};

/*
 * このクラスの使用者は必ずwait()を呼ぶこと。
 * でないとterminateする。
 */
class Worker {
	std::thread worker_thread;
public:
	explicit
	Worker(WorkQueue& work_queue) :
			worker_thread([&]() {do_works(work_queue);}) {
	}

	Worker(const Worker&) = delete;
	Worker(Worker&&) = delete;

	void wait() {
		worker_thread.join();
	}
	

private:
	static void do_works(WorkQueue& work_queue) {


		printf("woker is wake up!\n");

		for (;;) {
			ThreadData* thread_data = work_queue.pop();
			printf("pop %p\n", thread_data);
			if (!thread_data) {
				printf("worker will quit\n");
				return;
			}

			printf("stack frame is at %p\n", thread_data->stack_frame.get());

			execute_thread(*thread_data);
		}

	}

};

namespace {
constexpr size_t stack_size = 0xffff;
// WorkQueue work_queue { 1 };
thread_local context worker_thread_context;
thread_local ThreadData* current_thread;
void entry_thread(ThreadData& thread_data) {
	printf("start thread in new stack frame\n");
	std::cout << std::endl;
	thread_data.state = ThreadState::running;

	thread_data.func(thread_data.arg);

	printf("end thread\n");
	thread_data.state = ThreadState::ended;
	printf("end: %p\n", &thread_data);

	// jump to last worker context
	mylongjmp(worker_thread_context);
	// no return
}

class WorkerManager {
	WorkQueue work_queue;
	std::list<Worker> workers;

	std::mutex mutex_for_threads;
	std::list<ThreadData> threads;

public:
	explicit
	WorkerManager(unsigned int number_of_worker) :
			work_queue(number_of_worker) {

		for (int i = 0; i < number_of_worker; ++i) {
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
			auto lk = make_unique_lock(mutex_for_threads);
			threads.emplace_back(func, arg,
					std::make_unique<char[]>(stack_size));
			thread_data = &threads.back();
		}

		work_queue.push(*thread_data);

		return Thread(*thread_data);

	}
};

void execute_thread(ThreadData& thread_data) {

	printf("start executing user thread!\n");
	if (mysetjmp(worker_thread_context)) {
		printf("jumped to worker\n");
		current_thread = nullptr;
		return;
	}

	current_thread = &thread_data;

	if (thread_data.state == ThreadState::before_launch) {
		char* stack_frame = thread_data.stack_frame.get();
		printf("launch user thread!\n");
		__asm__("movq %0, %%rsp" : : "r" (stack_frame + stack_size) : "%rsp");
		entry_thread(thread_data);
	} else {
		thread_data.state = ThreadState::running;
		mylongjmp(thread_data.env);
	}

}
}

void yield_thread() {
	if (current_thread == nullptr) {
		throw std::logic_error("bad operation: yield worker thread");
	}

	printf("y: %p\n", current_thread);

	current_thread->state = ThreadState::stop;

	if (mysetjmp(current_thread->env)) {
		printf("jump back!\n");
		return;
	}
	mylongjmp(worker_thread_context);
}

//void join_thread(Thread& thread) {
//
//}

Thread start_thread(void (*)(void*), void*) {
	throw "not implemented";
}

/**
 * This function blocks until all user threads finish.
 */
void start_main_thread(void (*func)(void* arg), void* arg) {

}

void childfun(void* arg);

void mainfun(void* arg) {

	auto ch0 = start_thread(childfun, 0);
	// join_thread(ch0);

}

void childfun(void* arg) {
	printf("childfun!\n");
	yield_thread();

	printf("childfun 1 \n");
	yield_thread();
	printf("childfun 2 \n");
	yield_thread();
	printf("childfun 3 \n");
	yield_thread();
	printf("childfun end!\n");
}
