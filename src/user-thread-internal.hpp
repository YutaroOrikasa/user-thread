#ifndef USER_THREAD_INTERNAL_HPP_
#define USER_THREAD_INTERNAL_HPP_

#include <cstdio>
#include <list>
#include <thread>
#include <condition_variable>
#include <memory>
#include <iostream>

#include <boost/range/irange.hpp>

#include "mysetjmp.h"
#include "user-thread-debug.hpp"
#include "workqueue.hpp"
#include "stackallocators.hpp"

#include "call_with_alt_stack_arg3.h"

#include "config.h"

namespace orks {
namespace userthread {
namespace detail {

#ifdef ORKS_USERTHREAD_STACK_ALLOCATOR
using StackAllocator = ORKS_USERTHREAD_STACK_ALLOCATOR;
#else
using StackAllocator = SimpleStackAllocator;
#endif
using Stack = StackAllocator::Stack;


enum class ThreadState {
    running, ended, before_launch, stop
};

struct ThreadData {
    void (*func)(void* arg);
    void* arg;
    const Stack stack_frame;
    context env;
    ThreadState state = ThreadState::before_launch;

public:
    ThreadData(void (*func)(void* arg), void* arg, Stack stack_frame)
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


inline
void call_with_alt_stack_arg3(char* altstack, std::size_t altstack_size, void* func, void* arg1, void* arg2, void* arg3) {
    void* stack_base = altstack + altstack_size;
    debug::printf("func %p\n", func);
    orks_private_call_with_alt_stack_arg3_impl(arg1, arg2, arg3, stack_base, func);
}

inline
void call_with_alt_stack_arg3(Stack& altstack, void* func, void* arg1, void* arg2, void* arg3) {
    call_with_alt_stack_arg3(altstack.stack.get(), altstack.size, func, arg1, arg2, arg3);
}


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
    Stack alternative_stack = {};

    std::thread worker_thread;




public:
    explicit Worker(WorkQueue work_queue, std::string worker_name = "") :
        work_queue(work_queue) {
        alternative_stack = StackAllocator::allocate();

        worker_thread = std::thread([this, worker_name]() {
            do_works(worker_name);
        });
    }

    Worker(const Worker&) = delete;
    Worker(Worker&&) = delete;

    void wait() {
        worker_thread.join();
    }

    void schedule_thread() {

        auto this_thread = current_thread;
        assert(this_thread != nullptr);
        debug::printf("yield from: %p\n", this_thread);

        this_thread->state = ThreadState::stop;
        if (mysetjmp(this_thread->env)) {
            this_thread->state = ThreadState::running;
            return;
        }
        execute_next_thread(*this);

    }

    void create_thread(ThreadData& t) {

        auto this_thread = current_thread;
        assert(this_thread != nullptr);
        debug::printf("thread %p created at %p!\n", &t, this_thread);
        this_thread->state = ThreadState::stop;
        if (mysetjmp(this_thread->env)) {
            this_thread->state = ThreadState::running;
            return;
        }
        // mylongjmp(this_thread->env);
        execute_next_thread(*this, &t);
    }

private:
    void do_works(std::string worker_name) {

        register_worker_of_this_native_thread(*this, worker_name);

        debug::printf("worker is wake up! this: %p\n", this);

        if (mysetjmp(worker_thread_context)) {
            debug::printf("worker will quit\n");
            return;
        }

        execute_next_thread(*this);
        // never_come_here
        assert(false);

    }


//    void execute_thread(ThreadData& thread_to_execute, ThreadData* previous_thread = nullptr) {
//
//        // called at an user thread
//
//        // at previous_thread context
//        debug::printf("execute_thread()\n");
//        auto& previous_env = previous_thread ? previous_thread->env : worker_thread_context;
//        if (mysetjmp(previous_env)) {
//            // at previous_thread context
//            debug::printf("jumped back to execute_thread() on previous_thread\n");
//            auto& worker = get_worker_of_this_native_thread();
//            auto thread_jump_from = worker.current_thread;
//            if (thread_jump_from != nullptr) {
//                debug::out << "jump from " << thread_jump_from << std::endl;
//            }
//            worker.call_me_after_context_switch(previous_thread, thread_jump_from);
//
//            // return to [previous_thread context]schedule_thread() or [worker context]do_works()
//            debug::printf("return to context %p\n", previous_thread);
//            return;
//        }
//
//        // 開始/再開 した先のスレッドで push([this context]) / delete [this context] する必要がある。
//
//        if (thread_to_execute.state == ThreadState::before_launch) {
//            char* stack_frame = thread_to_execute.stack_frame.get();
//            debug::printf("launch user thread!\n");
//
//            __asm__("movq %0, %%rsp" : : "r"(stack_frame + stack_size) : "%rsp");
//            // at the context of thread_to_execute
//
//            // DO NOT touch local variable because the address of stack frame has been changed.
//            // you can touch function argument because they are in register.
//            entry_thread(thread_to_execute, previous_thread);
//
//        } else {
//            debug::printf("resume user thread!\n");
//            thread_to_execute.state = ThreadState::running;
//            mylongjmp(thread_to_execute.env);
//        }
//
//    }

    static void execute_next_thread(Worker& worker, ThreadData* next = nullptr) {
        debug::printf("execute_next_thread_impl %p\n", execute_next_thread_impl);
        call_with_alt_stack_arg3(worker.alternative_stack, reinterpret_cast<void*>(execute_next_thread_impl), &worker, next, nullptr);
//        __asm__("movq %0, %%rsp" : : "r") : "%rsp");
//        execute_next_thread_impl(worker, next);

    }

    static void execute_next_thread_impl(Worker& worker_before_switch, ThreadData* p_next_thread) {


        ThreadData* previous_thread = worker_before_switch.current_thread;

        if (!p_next_thread) {
            // pop before push
            p_next_thread = worker_before_switch.work_queue.pop();
        }

        if (previous_thread) {
            if (previous_thread->state == ThreadState::ended) {
                // delete previous_thread
            } else {
                // push after pop
                worker_before_switch.work_queue.push(*previous_thread);
            }
        }


        // guard
        if (!p_next_thread) {
            worker_before_switch.current_thread = nullptr;
            mylongjmp(worker_before_switch.worker_thread_context);
        }

        ThreadData& next_thread = *p_next_thread;
        debug::printf("execute thread %p, stack frame is %p\n", &next_thread, next_thread.stack_frame.stack.get());

        worker_before_switch.current_thread = &next_thread;

        // at previous_thread context
//        if (mysetjmp(previous_thread->env)) {
//            // at previous_thread context
//            debug::printf("jumped back to execute_thread() on previous_thread\n");
//            auto& worker_after_switch = get_worker_of_this_native_thread();
//            auto thread_jump_from = worker_after_switch.current_thread;
//            if (thread_jump_from != nullptr) {
//                debug::out << "jump from " << thread_jump_from << std::endl;
//            }
//            worker_after_switch.call_me_after_context_switch(previous_thread, thread_jump_from);
//
//            // return to [previous_thread context]schedule_thread() or [worker context]do_works()
//            debug::printf("return to context %p\n", previous_thread);
//            return;
//        }

        if (next_thread.state == ThreadState::before_launch) {
            char* stack_frame = next_thread.stack_frame.stack.get();
            debug::printf("launch user thread!\n");

            __asm__("movq %0, %%rsp" : : "r"(stack_frame + next_thread.stack_frame.size) : "%rsp");
            // at the context of next_thread

            // DO NOT touch local variable because the address of stack frame has been changed.
            // you can touch function argument because they are in register.
            call_with_alt_stack_arg3(stack_frame, next_thread.stack_frame.size, reinterpret_cast<void*>(entry_thread), &next_thread, nullptr, nullptr);

        } else {
            debug::printf("resume user thread %p!\n", &next_thread);
            next_thread.state = ThreadState::running;
            mylongjmp(next_thread.env);
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

        execute_next_thread(worker);
        // no return
        // this thread context will be deleted by next thread
    }

//    void call_me_after_context_switch(ThreadData* thread_now, ThreadData* thread_jump_from) {
//
//
//        if (!thread_jump_from) {
//            current_thread = thread_now;
//            return;
//        }
//
//
//
//        if (thread_jump_from->state != ThreadState::ended) {
//            // come here when yield() called
//            // current_thread is one that called yield
//            work_queue.push(*thread_jump_from);
//        } else {
//            // delete thread_jump_from;
//        }
//
//        // current_thread can be nullptr when thread_now is worker thread (thread_jump_from == nullptr)
//        current_thread = thread_now;
//
//    }

};

} // detail
} // userthread
} // orks



#endif /* USER_THREAD_INTERNAL_HPP_ */
