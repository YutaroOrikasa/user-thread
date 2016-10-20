#include <cstdio>
#include <setjmp.h>
#include <malloc.h>

#include <memory>
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
public:

	bool running();
	void start();
	void continue_();

};

void yield_thread();
Thread start_thread(void (*func)());

#include <list>

void childfun() {
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


int main(int argc, char **argv) {
	printf("aaa\n");
	// jmp_buf env;
	
	printf("call 3 childfun\n");
	std::list<Thread> threads;
	threads.push_back(start_thread(childfun));
	threads.push_back(start_thread(childfun));
	threads.push_back(start_thread(childfun));

	while (!threads.empty()) {
		for (auto it = threads.begin(), end = threads.end(); it != end;) {
			auto& thread = *it;
			printf("c: %p\n", &thread);
			if (thread.running()) {
				printf("continue childfun\n");
				thread.continue_();
			} else {
				printf("childfun finished\n");
				it = threads.erase(it);
				continue;

			}
			++it;
		}
	}

	printf("end 3 childfun\n");
}

/******************** internal *******************/

#include <list>
#include <memory>
#include <stdexcept>


namespace {
constexpr size_t stack_size = 0xffff;


jmp_buf main_thread_env;
ThreadData* current_thread = nullptr;

}



Thread start_thread(void (*func)()) {
	Thread th;
	th.thread_data = std::make_unique<ThreadData>();

	th.thread_data->func = func;
	th.thread_data->stack_frame = std::make_unique<char[]>(stack_size);


	th.start();
	return th;

}

void yield_thread() {
	if (current_thread == nullptr) {
		throw std::logic_error("bad operation: yield main thread");
	}

	printf("y: %p\n", current_thread);

	if (setjmp(current_thread->env)) {
		printf("jump back!\n");
		return;
	}
	longjmp(main_thread_env, 1);
}


void thread_entry(ThreadData& thread_data) {
	printf("start thread in new stack frame\n");

	thread_data.state = ThreadState::running;

	thread_data.func();

	printf("end thread\n");
	thread_data.state = ThreadState::ended;
	printf("end: %p\n", &thread_data);

	longjmp(main_thread_env, 1);
	// no return
}


void Thread::start() {



	if (setjmp(main_thread_env)) {
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
	thread_entry(*thread_data);

}

void Thread::continue_() {
	if (setjmp(main_thread_env)) {
		printf("jumped to start thread function\n");
		current_thread = nullptr;
		return;
	}

	thread_data->state = ThreadState::running;
	current_thread = &*thread_data;

	longjmp(thread_data->env, 1);
}

bool Thread::running() {
	return thread_data->state == ThreadState::running;
}


