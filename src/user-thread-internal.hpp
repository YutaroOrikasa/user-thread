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

#include <boost/range/irange.hpp>

#include "config.h"
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

class Worker;
void register_worker_of_this_native_thread(Worker& worker, std::string worker_name = "");
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

template<typename T>
class ThreadSafeQueue {
    std::mutex mutex;
    std::queue<T> queue;

public:

    void push(const T& t) {
        auto lock = util::make_unique_lock(mutex);
        queue.push(t);
    }

    /*
     * return true if queue is not empty
     * return false if else
     */
    bool pop(T& t) {
        auto lock = util::make_unique_lock(mutex);
        if (queue.empty()) {
            return false;
        }

        t = queue.front();
        queue.pop();
        return true;
    }

};


/*
 * pop時にnullptrが返った場合はqueueがcloseされたことを表す。
 */
template<typename T,
         template<typename U> class ThreadSafeQueue = ThreadSafeQueue>
class WorkStealQueue {
    std::vector<ThreadSafeQueue<T*>> work_queues;
    std::atomic_bool closed = { false };


public:

    class WorkQueue {
        ThreadSafeQueue<T*>& queue;
        WorkStealQueue& wsq;
        int queue_num;

    public:
        explicit WorkQueue(ThreadSafeQueue<T*>& q, WorkStealQueue& wsq,
                           int queue_num) :
            queue(q), wsq(wsq), queue_num(queue_num) {

        }
        void push(T& t) {
            debug::printf("WorkQueue::push %p\n", &t);
            queue.push(&t);
        }

        T* pop() {

            T* t;
            if (queue.pop(t)) {
                debug::printf("WorkQueue::pop %p\n", t);
                return t;
            }

            return wsq.steal();
        }

        void close() {
            wsq.close();
        }

        bool is_closed() {
            return wsq.is_closed();
        }

    };

    explicit WorkStealQueue(int num_of_worker) :
        work_queues(num_of_worker) {

    }

    /*
     * return thread local work queue that can steal work from other work queue
     * *must* index < num_of_worker
     */
    WorkQueue get_local_queue(int index) {
        return WorkQueue { work_queues.at(index), *this, index };
    }


    /*
     * return false if no works left
     * return true if else
     */
    T* steal() {

        for (;;) { //for (auto i : boost::irange(0, 100)) {
            debug::printf("WorkQueue::steal loop\n");
            if (closed) {
                debug::printf("WorkQueue::steal closed\n");
                return nullptr;
            }


            for (int i : boost::irange(0, static_cast<int>(work_queues.size()))) {
                auto& queue = work_queues[i];
                T* t;
                if (queue.pop(t)) {
                    debug::printf("WorkQueue::steal %p\n", t);
                    return t;
                }
            }

        }
        return nullptr;
    }

    void close() {
        closed = true;
    }

    bool is_closed() {
        return closed;
    }

};


/*
 * main thread でworker を 1つ 作成すると、新しい native thread が1つ作成される。
 * このクラスの使用者は必ずwait()を呼ぶこと。
 * でないとterminateする。
 */
class Worker {
    using WorkQueue = WorkStealQueue<ThreadData>::WorkQueue;
    WorkQueue work_queue;

    context worker_thread_context;
    ThreadData* volatile current_thread = nullptr;
    std::thread worker_thread;

public:
    explicit Worker(WorkQueue work_queue, std::string worker_name = "") :
        work_queue(work_queue),
        worker_thread([this, worker_name]() {
        do_works(worker_name);
    }) {
    }

    Worker(const Worker&) = delete;
    Worker(Worker&&) = delete;

    void wait() {
        worker_thread.join();
    }

    void schedule_thread() {

        // switch した先のスレッドで push([this context]) する必要がある。

        if (current_thread == nullptr) {
            debug::printf("err at this: %p\n", this);
            throw std::logic_error("bad operation: yield worker thread");
        }

        debug::printf("yield from: %p\n", current_thread);

        current_thread->state = ThreadState::stop;

        auto next_thread = work_queue.pop();

        if (next_thread) {
            execute_thread(*next_thread, current_thread);
        }

    }

    void create_thread(ThreadData& t) {

        // create した先のスレッドで push([this context]) する必要がある。

        assert(current_thread != nullptr);
        debug::printf("switch from %p to %p!\n", current_thread, &t);
        current_thread->state = ThreadState::stop;
        if (mysetjmp(current_thread->env)) {
            current_thread->state = ThreadState::running;
        }
        execute_thread(t, current_thread);
    }

private:
    void do_works(std::string worker_name) {

        register_worker_of_this_native_thread(*this, worker_name);

        debug::printf("worker is wake up! this: %p\n", this);

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


    void execute_thread(ThreadData& thread_to_execute, ThreadData* previous_thread = nullptr) {

        // called at an user thread

        // at previous_thread context
        debug::printf("execute_thread()\n");
        auto& previous_env = previous_thread ? previous_thread->env : worker_thread_context;
        if (mysetjmp(previous_env)) {
            // at previous_thread context
            debug::printf("jumped back to execute_thread() on previous_thread\n");
            auto& worker = get_worker_of_this_native_thread();
            auto thread_jump_from = worker.current_thread;
            if (thread_jump_from != nullptr) {
                debug::out << "jump from " << thread_jump_from << std::endl;
            }
            worker.call_me_after_context_switch(previous_thread, thread_jump_from);

            // return to [previous_thread context]schedule_thread() or [worker context]do_works()
            debug::printf("return to context %p\n", previous_thread);
            return;
        }

        // 開始/再開 した先のスレッドで push([this context]) / delete [this context] する必要がある。

        if (thread_to_execute.state == ThreadState::before_launch) {
            char* stack_frame = thread_to_execute.stack_frame.get();
            debug::printf("launch user thread!\n");

            __asm__("movq %0, %%rsp" : : "r"(stack_frame + stack_size) : "%rsp");
            // at the context of thread_to_execute

            // DO NOT touch local variable because the address of stack frame has been changed.
            // you can touch function argument because they are in register.
            entry_thread(thread_to_execute, previous_thread);

        } else {
            debug::printf("resume user thread!\n");
            thread_to_execute.state = ThreadState::running;
            mylongjmp(thread_to_execute.env);
        }

    }

    __attribute__((noinline))
    static void entry_thread(ThreadData& thread_data, ThreadData* previous_thread_data) {

        get_worker_of_this_native_thread().call_me_after_context_switch(&thread_data, previous_thread_data);

        debug::printf("start thread in new stack frame\n");
        debug::out << std::endl;
        thread_data.state = ThreadState::running;

        thread_data.func(thread_data.arg);

        debug::printf("end thread\n");
        thread_data.state = ThreadState::ended;
        debug::printf("end: %p\n", &thread_data);

        // worker can switch before and after func() called.
        auto& worker = get_worker_of_this_native_thread();

        auto next_thread = worker.work_queue.pop();

        if (next_thread) {

            // 開始/再開 した先のスレッドで delete [this context] する必要がある。

            debug::printf("jump to next thread %p\n", next_thread);
            worker.execute_thread(*next_thread, &thread_data);
        } else {
            debug::printf("jump to worker context\n");
            mylongjmp(worker.worker_thread_context);
        }
        // no return
        // this thread context will be deleted by next thread
    }

    void call_me_after_context_switch(ThreadData* thread_now, ThreadData* thread_jump_from) {


        if (!thread_jump_from) {
            current_thread = thread_now;
            return;
        }



        if (thread_jump_from->state != ThreadState::ended) {
            // come here when yield() called
            // current_thread is one that called yield
            work_queue.push(*thread_jump_from);
        } else {
            // delete thread_jump_from;
        }

        // current_thread can be nullptr when thread_now is worker thread (thread_jump_from == nullptr)
        current_thread = thread_now;

    }

};

} // detail
} // userthread
} // orks



#endif /* USER_THREAD_INTERNAL_HPP_ */
