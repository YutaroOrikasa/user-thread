#include <iostream>
#include "user-thread.hpp"

namespace orks {
namespace userthread {
namespace detail {

// private
namespace {
thread_local Worker* worker_of_this_native_thread;
}

void register_worker_of_this_native_thread(Worker& worker) {
	worker_of_this_native_thread = &worker;
}

Worker& get_worker_of_this_native_thread() {
	return *worker_of_this_native_thread;
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



