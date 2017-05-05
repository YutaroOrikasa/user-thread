#include <iostream>
#include <exception>
#include <utility>

#include "user-thread.hpp"

using namespace orks::userthread;

long fibo(long n) {
    // std::cout << "fibo(" << n << ")" << std::endl;
    if (n == 0 || n == 1) {
        return  n;
    }
    auto future = create_thread(fibo, n - 1);
    auto n2 = fibo(n - 2);
    for (; future.wait_for(std::chrono::seconds(0)) != std::future_status::ready;) {
        // std::cout << "wait fibo(" << n << "-1)" << std::endl;
        orks::userthread::yield();
    }

    return n2 + future.get();
}

void fibo_main(void*) {
    std::cout << "fibo(20) must be 6765. answer: " << fibo(20) << std::endl;
}

int main() {

    try {
        orks::userthread::start_main_thread(fibo_main, nullptr);
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "caught unknown exception" << std::endl;
        return 1;
    }



}
