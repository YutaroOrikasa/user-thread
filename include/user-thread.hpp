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

class Thread {
    detail::ThreadData* thread_data;

public:

    explicit Thread(detail::ThreadData& td) :
        thread_data(&td) {
    }

    bool running();

};

namespace detail {
class WorkerManager {
    WorkStealQueue<ThreadData> work_queue;
    std::list<Worker> workers;

    std::mutex mutex_for_threads;
    std::list<ThreadData> threads;

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
            workers.emplace_back(work_queue.get_local_queue(i));
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

        threads.emplace_back(exec_thread<decltype(main0)>, &main0, std::make_unique<char[]>(stack_size));
        auto& main_thread = threads.back();

        work_queue.get_local_queue(0).push(main_thread);

        for (auto& worker : workers) {
            worker.wait();
        }

    }

    Thread start_thread(void (*func)(void* arg), void* arg) {
        ThreadData* thread_data;
        {
            auto lk = util::make_unique_lock(mutex_for_threads);
            threads.emplace_back(func, arg,
                                 std::make_unique<char[]>(stack_size));
            thread_data = &threads.back();
        }

        get_worker_of_this_native_thread().create_thread(*thread_data);

        return Thread(*thread_data);

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

Thread start_thread(void (*func)(void* arg), void* arg);

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

namespace detail {

template<typename Fn>
void exec_thread(void* func_obj) {
    (*static_cast<Fn*>(func_obj))();
    delete static_cast<Fn*>(func_obj);
}
}

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
    orks::userthread::start_thread(detail::exec_thread<Fn0>, new Fn0(std::move(fn0)));
    return future;
}

}
}



#endif /* USER_THREAD_HPP_ */
