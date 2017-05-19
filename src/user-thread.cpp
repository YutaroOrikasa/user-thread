#include <memory>

#include "user-thread.hpp"

namespace orks {
namespace userthread {
namespace detail {
namespace debug {
std::mutex debug_out_mutex;
}
}
}
}

namespace orks {
namespace userthread {
namespace detail {

// private
namespace {
thread_local Worker* worker_of_this_native_thread;

std::unique_ptr<WorkerManager> worker_manager_ptr;
}

void register_worker_of_this_native_thread(Worker& worker) {
    worker_of_this_native_thread = &worker;
}

Worker& get_worker_of_this_native_thread() {
    return *worker_of_this_native_thread;
}

} // detail

using namespace detail;

void init_worker_manager(unsigned int number_of_worker) {
    if (!worker_manager_ptr) {
        worker_manager_ptr = std::make_unique<WorkerManager>(number_of_worker);
    }
}

void init_worker_manager() {
    if (!worker_manager_ptr) {
        worker_manager_ptr = std::make_unique<WorkerManager>();
    }
}

void start_main_thread(void (*func)(void*), void* arg) {
    init_worker_manager();
    worker_manager_ptr->start_main_thread(func, arg);
}

Thread start_thread(void (*func)(void*), void* arg) {
    return worker_manager_ptr->start_thread(func, arg);
}



void yield() {
    worker_manager_ptr->scheduling_yield();
}
}
}

//void join_thread(Thread& thread) {
//
//}



