#include <list>
#include <memory>
#include <stdexcept>

#include "user-thread.hpp"
#include "mysetjmp.h"

namespace {
constexpr size_t stack_size = 0xffff;

context main_thread_env;
ThreadData* current_thread = nullptr;

void thread_entry(ThreadData& thread_data, void* arg) {
	printf("start thread in new stack frame\n");

	thread_data.state = ThreadState::running;

	thread_data.func(arg);

	printf("end thread\n");
	thread_data.state = ThreadState::ended;
	printf("end: %p\n", &thread_data);

	mylongjmp(main_thread_env);
	// no return
}

}

Thread start_thread(void (*func)(void* arg), void* arg) {
	Thread th;
	th.thread_data = std::make_unique<ThreadData>();

	th.thread_data->func = func;
	th.thread_data->stack_frame = std::make_unique<char[]>(stack_size);

	th.start(arg);
	return th;

}


void yield_thread() {
	if (current_thread == nullptr) {
		throw std::logic_error("bad operation: yield main thread");
	}

	printf("y: %p\n", current_thread);

	if (mysetjmp(current_thread->env)) {
		printf("jump back!\n");
		return;
	}
	mylongjmp(main_thread_env);
}

void Thread::start(void* arg) {

	if (mysetjmp(main_thread_env)) {
		printf("jumped to start thread function\n");
		current_thread = nullptr;
		return;
	}

	thread_data->state = ThreadState::running;
	current_thread = &*thread_data;

	// DEBUG
	printf(":st this: %p\n", this);
	printf(":st: %p\n", &*thread_data);

	char* stack_frame = thread_data->stack_frame.get();
	__asm__("movq %0, %%rsp" : : "r" (stack_frame + stack_size) : "%rsp");
	thread_entry(*thread_data, arg);

}

void Thread::continue_() {
	if (mysetjmp(main_thread_env)) {
		printf("jumped to start thread function\n");
		current_thread = nullptr;
		return;
	}

	thread_data->state = ThreadState::running;
	current_thread = &*thread_data;

	mylongjmp(thread_data->env);
}

bool Thread::running() {
	return thread_data->state == ThreadState::running;
}
