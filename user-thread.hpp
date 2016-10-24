// notice: API is not stable!

#ifndef USER_THREAD_HPP_
#define USER_THREAD_HPP_

#include <memory>

#include "mysetjmp.h"

enum class ThreadState {
	running, ended
};

struct ThreadData {
	context env;
	ThreadState state;
	std::unique_ptr<char[]> stack_frame;
	void (*func)(void* arg);
	ThreadData() = default;

	// non copyable
	ThreadData(const ThreadData&) = delete;
	ThreadData(ThreadData&&) = delete;

};

class Thread {
	friend Thread start_thread(void (*func)(void* arg), void* arg);
	std::unique_ptr<ThreadData> thread_data;

	void start(void* arg);

public:

	bool running();
	void continue_();

};

void yield_thread();
Thread start_thread(void (*func)(void* arg), void* arg);



#endif /* USER_THREAD_HPP_ */
