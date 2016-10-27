// notice: API is not stable!

#ifndef USER_THREAD_HPP_
#define USER_THREAD_HPP_

#include <memory>

#include "mysetjmp.h"

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

void yield_thread();
Thread start_thread(void (*func)(void* arg), void* arg);



#endif /* USER_THREAD_HPP_ */
