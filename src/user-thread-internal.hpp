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
#include "workqueue.hpp"


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

using Work = BadDesignContextTraits::Context;
/*
 * main thread でworker を 1つ 作成すると、新しい native thread が1つ作成される。
 * このクラスの使用者は必ずwait()を呼ぶこと。
 * でないとterminateする。
 */
class Worker {
    using WorkQueue = WorkStealQueue<Work>::WorkQueue;

    using ContextTraits = BadDesignContextTraits;

    WorkQueue work_queue;

    Work volatile worker_thread_context = nullptr;

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

    void create_thread(Work t) {

        debug::printf("create thread %p\n", &t);
        switch_thread_to(t);

    }

    static Work make_thread(void (*func)(void* arg), void* arg) {
        auto func_ = [func, arg](Work prev) -> Work {
            call_after_context_switch(prev);

            func(arg);

            debug::printf("fini\n");
            auto& worker = get_worker_of_this_native_thread();
            auto pop_no_timeout = [&]() -> decltype(worker.work_queue.pop()) {
                while (!worker.work_queue.is_closed()) {
                    auto p_next = worker.work_queue.pop();
                    if (p_next) {
                        return p_next;
                    }
                }
                return boost::none;
            };
            auto p_next = pop_no_timeout();

            if (!p_next) {
                debug::printf("work queue was closed. will jump back to worker context\n");
                // remove volatile by copy
                auto tmp = worker.worker_thread_context;
                p_next = tmp;
            }
            debug::printf("next thread is at %p\n", p_next.get());
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

        while (!work_queue.is_closed()) {
            schedule_thread();
        }

        debug::printf("jumped back to worker context\n");
        debug::printf("worker will quit\n");

        assert(work_queue.is_closed());

    }


    void switch_thread(WorkQueue& work_queue) {
        auto p_next = work_queue.pop();
        if (!p_next) {
            if (!work_queue.is_closed()) {
                debug::printf("work queue was time out. no context switch will occur.\n");
                return;
            }

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

    void switch_thread_to(Work next) {

        debug::printf("jump to Work %p\n", &next);
        auto prev = switch_context(next);

        call_after_context_switch(prev);

    }

    Work switch_context(Work to) {
//        auto this_thread = current_thread;
//        this_thread->state = ThreadState::stop;
//        assert(this_thread != nullptr);
//#ifdef USE_SPLITSTACKS
//        __splitstack_getcontext(this_thread->splitstack_context_);
//#endif
//
        return ContextTraits::switch_context(to);
    }

    static void call_after_context_switch(Work prev) {
        Worker& worker = Worker::get_worker_of_this_native_thread();
        debug::printf("worker.worker_thread_context %p\n", worker.worker_thread_context);
        if (worker.worker_thread_context == nullptr) {
            debug::out << "prev Work is worker_thread_context\n";
            worker.worker_thread_context = prev;
            return;
        }

        if (ContextTraits::is_finished(prev)) {
            debug::printf("delete prev Work %p\n", &prev);
            ContextTraits::destroy_context(prev);
        } else {
            debug::printf("push prev Work %p\n", &prev);
            debug::out << "prev Work::state: " << static_cast<int>(prev->state) << "\n";
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
