#ifndef USER_THREAD_INTERNAL_HPP_
#define USER_THREAD_INTERNAL_HPP_

#include <cstdio>
#include <list>
#include <thread>
#include <condition_variable>
#include <memory>
#include <iostream>
#include <utility>

#include <boost/range/irange.hpp>

#include "stack-address-tools.hpp"
#include "context-traits.hpp"

namespace orks {
namespace userthread {
namespace detail {


template<typename Fn>
void exec_thread_delete(void* func_obj) {
    (*static_cast<Fn*>(func_obj))();
    delete static_cast<Fn*>(func_obj);
}

}
}
}


namespace orks {
namespace userthread {
namespace detail {

using namespace stacktool;


class Worker;
void register_worker_of_this_native_thread(Worker& worker, std::string worker_name = "");
Worker& get_worker_of_this_native_thread();

using Context = BadDesignContextTraits::Context;
/*
 * main thread でworker を 1つ 作成すると、新しい native thread が1つ作成される。
 * このクラスの使用者は必ずwait()を呼ぶこと。
 * でないとterminateする。
 */
class Worker {
    using WorkQueue = WorkStealQueue<Context>::WorkQueue;

    using ContextTraits = BadDesignContextTraits;
    friend void ContextTraits::set_current_thread(Context t);
    friend Context ContextTraits::get_current_thread();

    WorkQueue work_queue;

    Context volatile current_thread = nullptr;
    Context volatile worker_thread_context = nullptr;

    std::thread worker_thread;

public:
    explicit Worker(WorkQueue work_queue, std::string worker_name = "") :
        work_queue(work_queue) {
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

        switch_thread(work_queue);
    }

    void create_thread(Context t) {

        debug::printf("create thread %p\n", &t);
        switch_thread_to(t);

    }

    static Context make_thread(void (*func)(void* arg), void* arg) {
        auto func_ = [func, arg](Context prev) -> Context {
            call_after_context_switch(prev);
            func(arg);
            debug::printf("fini\n");
            auto& worker = get_worker_of_this_native_thread();
            auto p_next = worker.work_queue.pop();
            if (!p_next) {
                debug::printf("will jump back to worker context\n");
                // remove volatile by copy
                auto tmp = worker.worker_thread_context;
                p_next = tmp;
            }
            return p_next.get();
        };
        return BadDesignContextTraits::make_context(func_);
    }



private:
    void do_works(std::string worker_name) {
#ifdef USE_SPLITSTACKS
        __stack_split_initialize();
#endif

        register_worker_of_this_native_thread(*this, worker_name);

        debug::printf("worker is wake up! this: %p\n", this);
        debug::printf("worker_thread_context %p\n", worker_thread_context);

        schedule_thread();

        debug::printf("jumped back to worker context\n");
        debug::printf("worker will quit\n");

    }


    void switch_thread(WorkQueue& work_queue) {
        auto p_next = work_queue.pop();
        if (!p_next) {
            debug::printf("will jump back to worker context\n");
            if (worker_thread_context == nullptr) {
                return;
            }
            // remove volatile by copy
            auto tmp = worker_thread_context;
            p_next = tmp;
        }
        switch_thread_to(p_next.get());
    }

    void switch_thread_to(Context next) {

        debug::printf("jump to Context %p\n", &next);
        auto prev = switch_context(next);

        call_after_context_switch(prev);

    }

    Context switch_context(Context to) {
//        auto this_thread = current_thread;
//        this_thread->state = ThreadState::stop;
//        assert(this_thread != nullptr);
//#ifdef USE_SPLITSTACKS
//        __splitstack_getcontext(this_thread->splitstack_context_);
//#endif
//
        return ContextTraits::switch_context(to);
    }

    static void call_after_context_switch(Context prev) {
        Worker& worker = Worker::get_worker_of_this_native_thread();
        debug::printf("worker.worker_thread_context %p\n", worker.worker_thread_context);
        if (worker.worker_thread_context == nullptr) {
            debug::out << "prev Context is worker_thread_context\n";
            worker.worker_thread_context = prev;
            return;
        }

        if (ContextTraits::is_finished(prev)) {
            debug::printf("delete prev Context %p\n", &prev);
            ContextTraits::destroy_context(prev);
        } else {
            debug::printf("push prev Context %p\n", &prev);
            debug::out << "prev Context::state: " << static_cast<int>(prev->state) << "\n";
            worker.work_queue.push(prev);
        }
    }



    static Worker& get_worker_of_this_native_thread() {
        return ::orks::userthread::detail::get_worker_of_this_native_thread();
    }

};

} // detail
} // userthread
} // orks



#endif /* USER_THREAD_INTERNAL_HPP_ */
