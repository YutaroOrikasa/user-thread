// notice: API is not stable!

#ifndef USER_THREAD_HPP_
#define USER_THREAD_HPP_

#include <cstdio>
#include <list>
#include <queue>
#include <future>

#include "../src/user-thread-internal.hpp"


namespace orks {
namespace userthread {
namespace detail {
class WorkerManager {
    WorkStealQueue<ThreadData*> work_queue;
    std::list<Worker> workers;

    static unsigned int number_of_cpu_cores() {
        const auto num = std::thread::hardware_concurrency();
        if (num == 0) {
            return 1;
        }

        return num;
    }

public:
    explicit WorkerManager(unsigned int number_of_worker) :
        work_queue(number_of_worker) {

        for (unsigned int i = 0; i < number_of_worker; ++i) {
            workers.emplace_back(work_queue.get_local_queue(i), std::to_string(i));
        }
    }

    /*
     * construct WorkerManager with the number of the cpu cores of your computer
     */
    WorkerManager() :
        WorkerManager(number_of_cpu_cores()) {
    }

    /**
     * This function blocks until all user threads finish.
     */
    void start_main_thread(void (*func)(void* arg), void* arg) {

        // main_thread func wrapper
        auto main0 = [&]() {
            func(arg);
            this->work_queue.close();
        };

        auto dummy = [this]() {
            for (;;) {
                debug::printf("### dummy yield\n");
                this->scheduling_yield();
                debug::printf("### dummy resume\n");
                if (this->work_queue.is_closed()) {
                    debug::printf("### dummy quit\n");
                    return;
                }
            }
        };

        // created ThreadData* will be deleted in Worker::execute_next_thread_impl
        auto main_thread = Worker::make_thread(exec_thread<decltype(main0)>, &main0);
        work_queue.get_local_queue(0).push(main_thread);

        /*
         * yield() 時に無限ループに陥らないようにするためにworker数分の ダーミースレッドを用意する。
         * 例えば、8 workers で 8つのuser thread しか存在しない状態で、8つのuser thread が同時にyieldした場合、
         * queueにスレッドが１つも入っていない状態でstealしようとするので、無限ループが発生する。
         * 少なくともworker数+1個のスレッドが存在していれば無限ループにはならない。
         * main thread があることを考えると、worker数分のダミーがあれば良い。
         *
         */
        for (auto i : boost::irange(0ul, workers.size())) {
            static_cast<void>(i);
            // created ThreadData* will be deleted in Worker::execute_next_thread_impl
            auto dummy_thread = Worker::make_thread(exec_thread <decltype(dummy)> , &dummy);
            debug::printf("### push dummy thread\n");
            work_queue.get_local_queue(0).push(dummy_thread);
        }

        for (auto& worker : workers) {
            worker.wait();
        }

    }

    void start_thread(void (*func)(void* arg), void* arg) {


        // created ThreadData* will be deleted in Worker::execute_next_thread_impl
        ThreadData* thread_data = Worker::make_thread(func, arg);

        get_worker_of_this_native_thread().create_thread(*thread_data);

    }

    /**
     * user threadがこの関数を呼び出すと、呼び出したuser threadは一時停止し、他のuser threadが動く。
     * この関数を呼び出したuser threadはスケジューラーによって自動的に再開される。
     */
    void scheduling_yield() {
        get_worker_of_this_native_thread().schedule_thread();
    }

    // static utility function
    template<typename Fn>
    static void exec_thread(void* func_obj) {
        (*static_cast<Fn*>(func_obj))();
    }
};
}
using detail::WorkerManager;

/* 重要!
*  main thread が終了した後に新規スレッド作成かyieldをすると未定義動作
*  main thread がyield()を呼ぶと未定義動作
*/

void start_thread(void (*func)(void* arg), void* arg);

/*
* initialize global worker manager with the number of the worker.
* DO NOT call twice.
*/
void init_worker_manager(unsigned int number_of_worker);

/*
* initialize global worker manager with the number of the cpu cores.
* DO NOT call twice.
*/
void init_worker_manager();

/**
* This function blocks until all user threads finish.
*/
void start_main_thread(void (*func)(void* arg), void* arg);

void yield();

// return: std::future<auto>
template <typename Fn, typename... Args>
auto create_thread(Fn fn, Args... args) {
    std::promise<decltype(fn(args...))> promise;
    auto future = promise.get_future();

    // TODO INVOKE(DECAY_COPY(std::forward<F>(f)), DECAY_COPY(std::forward<Args>(args))...)
    auto fn0 = [promise = std::move(promise), fn, args...]() mutable {
        promise.set_value(fn(args...));
    };
    using Fn0 = decltype(fn0);
    orks::userthread::start_thread(detail::exec_thread_delete<Fn0>, new Fn0(std::move(fn0)));
    return future;
}

}
}



#endif /* USER_THREAD_HPP_ */
