#include <iostream>
#include <future>
#include <exception>
#include <utility>

#include "user-thread.hpp"

namespace mymain {
using namespace orks::userthread;

WorkerManager wm {4};
}

template <typename Fn>
void exec_thread(void* func_obj) {
    (*static_cast<Fn*>(func_obj))();
    delete static_cast<Fn*>(func_obj);
}

// return: std::future<auto>
template <typename Fn>
auto create_thread(Fn fn) {
    std::promise<decltype(fn())> promise;
    auto future = promise.get_future();

    // TODO INVOKE(DECAY_COPY(std::forward<F>(f)), DECAY_COPY(std::forward<Args>(args))...)
    auto fn0 = [promise = std::move(promise), fn]() mutable {
        promise.set_value(fn());
    };
    using Fn0 = decltype(fn0);
    mymain::wm.start_thread(exec_thread<Fn0>, new Fn0(std::move(fn0)));
    return future;
}

long fibo(long n) {
    // std::cout << "fibo(" << n << ")" << std::endl;
    if (n == 0 || n == 1) {
        return  n;
    }
    auto future = create_thread([n]() {
        return fibo(n - 1);
    });
    auto n2 = fibo(n - 2);
    for (; future.wait_for(std::chrono::seconds(0)) != std::future_status::ready;) {
        // std::cout << "wait fibo(" << n << "-1)" << std::endl;
        mymain::wm.scheduling_yield();
    }

    return n2 + future.get();
}

void fibo_main(void*) {
    std::cout << "fibo(20) must be 6765. answer: " << fibo(20) << std::endl;
}

int main() {

    try {
        mymain::wm.start_main_thread(fibo_main, nullptr);
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "caught unknown exception" << std::endl;
        return 1;
    }



}
