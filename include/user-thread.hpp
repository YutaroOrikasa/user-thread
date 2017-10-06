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
    WorkStealQueue<Work> work_queue;
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

        // created Work* will be deleted in Worker::execute_next_thread_impl
        auto main_thread = Worker::make_thread(exec_thread<decltype(main0)>, &main0);
        work_queue.get_local_queue(0).push(main_thread);

        for (auto& worker : workers) {
            worker.wait();
        }

    }

    void start_thread(void (*func)(void* arg), void* arg) {


        // created Work* will be deleted in Worker::execute_next_thread_impl
        Work thread_data = Worker::make_thread(func, arg);

        get_worker_of_this_native_thread().create_thread(thread_data);

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

template <typename Ret>
struct call_and_set_value_to_promise_impl {
    template <class P, typename Fn, typename... Args>
    static
    void call_and_set_value_to_promise(P& promise, Fn fn, Args...args) {
        promise.set_value(fn(std::move(args)...));
    }
};

template <>
struct call_and_set_value_to_promise_impl<void> {
    template <class P, typename Fn, typename... Args>
    static
    void call_and_set_value_to_promise(P& promise, Fn fn, Args...args) {
        fn(std::move(args)...);
        promise.set_value();
    }
};

template <class P, typename Fn, typename... Args>
void call_and_set_value_to_promise(P& promise, Fn fn, Args...args) {
    call_and_set_value_to_promise_impl<decltype(fn(std::move(args)...))>::call_and_set_value_to_promise(promise, fn, std::move(args)...);
}

// return: std::future<auto>
template <typename Fn, typename... Args>
auto create_thread(WorkerManager& wm, Fn fn, Args... args) {
    std::promise<decltype(fn(args...))> promise;
    auto future = promise.get_future();

    // TODO INVOKE(DECAY_COPY(std::forward<F>(f)), DECAY_COPY(std::forward<Args>(args))...)
    auto fn0 = [promise = std::move(promise), fn, args...]() mutable {
        call_and_set_value_to_promise(promise, fn, std::move(args)...);
    };
    using Fn0 = decltype(fn0);
    wm.start_thread(detail::exec_thread_delete<Fn0>, new Fn0(std::move(fn0)));
    return future;
}

// blocks until main thread finished
// return: std::future<auto>
template <typename Fn, typename... Args>
auto start_main_thread(WorkerManager& wm, Fn fn, Args... args) {

    std::promise<decltype(fn(args...))> promise;
    auto future = promise.get_future();

    // TODO INVOKE(DECAY_COPY(std::forward<F>(f)), DECAY_COPY(std::forward<Args>(args))...)
    auto fn0 = [promise = std::move(promise), fn = std::move(fn), &args...]() mutable {
        call_and_set_value_to_promise(promise, fn, std::move(args)...);
    };
    using Fn0 = decltype(fn0);
    wm.start_main_thread(WorkerManager::exec_thread<Fn0>, &fn0);
    return future;

}

WorkerManager& get_global_workermanager();
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

    return detail::create_thread(detail::get_global_workermanager(), std::move(fn), std::move(args)...);
}

}
}



#endif /* USER_THREAD_HPP_ */
