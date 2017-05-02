#ifndef USER_THREAD_INTERNAL_HPP_
#define USER_THREAD_INTERNAL_HPP_

#include <cstdio>
#include <list>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <iostream>

#include "mysetjmp.h"
#include "user-thread-debug.hpp"

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
               std::unique_ptr<char[]> stack_frame)
        : func(func)
        , arg(arg)
        , stack_frame(std::move(stack_frame)) {
    }

    // non copyable
    ThreadData(const ThreadData&) = delete;
    ThreadData(ThreadData&&) = delete;

};
void execute_thread(ThreadData& thread_data);

class Worker;
void register_worker_of_this_native_thread(Worker& worker);
Worker& get_worker_of_this_native_thread();

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

    bool closed = false;

public:

    explicit WorkQueue(int number_of_workers) :
        number_of_workers(number_of_workers) {
    }

    // you can not push nullptr
    void push(ThreadData& td) {
        if (closed) {
            throw std::logic_error("pushing closed work queue");
        }
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
            if (queue.empty() && number_of_hungry_workers == number_of_workers) {
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

        debug::printf("WQ::pop %p\n", td);
        return td;
    }

private:
    void push_impl(ThreadData* td, bool notify_all) {

        auto lock = util::make_unique_lock(mutex);
        debug::printf("WQ::push %p\n", td);
        is_queue_started = true;
        queue.push(td);

        if (notify_all) {
            cv.notify_all();
        } else {
            cv.notify_one();
        }
    }

    void close() {
        debug::printf("WQ::close\n");
        push_impl(nullptr, true);
        closed = true;
    }

    /*
     * 内部仕様
     * すべてのworkerがpopしようとしたことが検出されたらwork queueをcloseする。
     */

};


/*
 * main thread でworker を 1つ 作成すると、新しい native thread が1つ作成される。
 * このクラスの使用者は必ずwait()を呼ぶこと。
 * でないとterminateする。
 */
class Worker {

    WorkQueue& work_queue;

    context worker_thread_context;
    ThreadData* current_thread = nullptr;

    std::thread worker_thread;

public:
    explicit Worker(WorkQueue& work_queue) :
        work_queue(work_queue),
        worker_thread([ & ]() {
        do_works();
    }) {
    }

    Worker(const Worker&) = delete;
    Worker(Worker&&) = delete;

    void wait() {
        worker_thread.join();
    }

    void schedule_thread() {

        if (current_thread == nullptr) {
            debug::printf("err at this: %p\n", this);
            throw std::logic_error("bad operation: yield worker thread");
        }

        debug::printf("yield from: %p\n", current_thread);

        current_thread->state = ThreadState::stop;

        if (mysetjmp(current_thread->env)) {
            debug::printf("jump back!\n");
            return;
        }
        mylongjmp(worker_thread_context);
    }

private:
    void do_works() {

        register_worker_of_this_native_thread(*this);

        debug::printf("worker is wake up!\n");

        for (;;) {
            ThreadData* thread_data = work_queue.pop();
            debug::printf("this %p pop %p\n", this, thread_data);
            if (!thread_data) {
                debug::printf("worker will quit\n");
                return;
            }

            debug::printf("stack frame is at %p\n", thread_data->stack_frame.get());

            execute_thread(*thread_data);

        }

    }

    void execute_thread(ThreadData& thread_data) {

        debug::printf("start executing user thread!\n");
        if (mysetjmp(worker_thread_context)) {
            debug::printf("jumped to worker\n");
            debug::out << "current_thread after jump: " << current_thread << std::endl;
            if (current_thread) {
                // come here when yield() called
                work_queue.push(*current_thread);
            }
            return;
        }

        current_thread = &thread_data;

        if (thread_data.state == ThreadState::before_launch) {
            char* stack_frame = thread_data.stack_frame.get();
            debug::printf("launch user thread!\n");

            __asm__("movq %0, %%rsp" : : "r"(stack_frame + stack_size) : "%rsp");
            // DO NOT touch local variable because the address of stack frame has been changed.
            // you can touch function argument because they are in register.
            entry_thread(thread_data);

        } else {
            thread_data.state = ThreadState::running;
            mylongjmp(thread_data.env);
        }

    }

    static void entry_thread(ThreadData& thread_data) {
        debug::printf("start thread in new stack frame\n");
        debug::out << std::endl;
        thread_data.state = ThreadState::running;

        thread_data.func(thread_data.arg);

        debug::printf("end thread\n");
        thread_data.state = ThreadState::ended;
        debug::printf("end: %p\n", &thread_data);

        // worker can switch before and after func() called.
        auto& worker = get_worker_of_this_native_thread();

        worker.current_thread = nullptr;
        // jump to last worker context
        mylongjmp(worker.worker_thread_context);
        // no return
    }

};

} // detail
} // userthread
} // orks



#endif /* USER_THREAD_INTERNAL_HPP_ */
