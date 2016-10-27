#ifndef USER_THREAD_INTERNAL_HPP_
#define USER_THREAD_INTERNAL_HPP_

#include <cstdio>
#include <list>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>

#include "mysetjmp.h"

namespace orks {
namespace userthread {
namespace detail {

constexpr size_t stack_size = 0xffff;
enum class ThreadState {
	running, ended, before_launch, stop
};

struct ThreadData {
	void (*func)(void* arg);
	void* arg;
	const std::unique_ptr<char[]> stack_frame;
	context env;
	ThreadState state = ThreadState::before_launch;

public:
	ThreadData(void (*func)(void* arg), void* arg,
			std::unique_ptr<char[]> stack_frame) :
			func(func), arg(arg), stack_frame(std::move(stack_frame)) {
	}

	// non copyable
	ThreadData(const ThreadData&) = delete;
	ThreadData(ThreadData&&) = delete;

};
void execute_thread(ThreadData& thread_data);

// internal helper
namespace util {

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
} // util

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

	explicit WorkQueue(int number_of_workers) :
			number_of_workers(number_of_workers) {
	}

	// you can not push nullptr
	void push(ThreadData& td) {
		push_impl(&td, false);
	}

	ThreadData* pop() {
		auto lock = util::make_unique_lock(mutex);
		++number_of_hungry_workers;
		auto finally = util::make_scope_exit([&]() {
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

		auto lock = util::make_unique_lock(mutex);
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
	explicit Worker(WorkQueue& work_queue) :
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

} // detail
} // userthread
} // orks



#endif /* USER_THREAD_INTERNAL_HPP_ */
