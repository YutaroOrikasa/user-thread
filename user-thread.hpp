// notice: API is not stable!

#ifndef USER_THREAD_HPP_
#define USER_THREAD_HPP_

#include <memory>
#include <setjmp.h>

enum class ThreadState {
	running, ended
};

struct ThreadData {
	jmp_buf env;
	ThreadState state;
	std::unique_ptr<char[]> stack_frame;
	void (*func)();
	ThreadData() = default;

	// non copyable
	ThreadData(const ThreadData&) = delete;
	ThreadData(ThreadData&&) = delete;

};

class Thread {
	friend Thread start_thread(void (*func)());
	std::unique_ptr<ThreadData> thread_data;

	void start();

public:

	bool running();
	void continue_();

};

void yield_thread();
Thread start_thread(void (*func)());



#endif /* USER_THREAD_HPP_ */
