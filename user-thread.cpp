#include <iostream>
#include "user-thread.hpp"

namespace orks {
namespace userthread {
namespace detail {

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

} // detail

Thread start_thread(void (*)(void*), void*) {
	throw "not implemented";
}

/**
 * This function blocks until all user threads finish.
 */
void start_main_thread(void (*func)(void* arg), void* arg) {
	throw "not implemented";
}
}
}

//void join_thread(Thread& thread) {
//
//}



