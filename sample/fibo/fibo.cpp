#include <iostream>
#include <exception>
#include <utility>
#include <string>

#include "user-thread.hpp"

using namespace orks::userthread;

long fibo(long n) {

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

void fibo_main(void* int_ptr) {
    if (!int_ptr) {
        std::cout << "fibo(20) must be 6765. answer: " << fibo(20) << std::endl;
    } else {
        int n = *static_cast<int*>(int_ptr);
        std::cout << "fibo(" << n << ") is: " << fibo(n) << std::endl;
    }
}
/*
 * usage:
 * $ ./fibo 4    # parallel processing with 4 workers
 * $ ./fibo      # parallel processing with cpu cores numbers workers
 * $ ./fibo 4 10 # calc fibo(10) with 4 workers
 */
int main(int argc, char** argv) {

    try {
        int worker_size = [&]() {
            if (argc >= 2) {
                return std::stoi(std::string(argv[1]));
            } else {
                return 0;
            }
        }();
        if (worker_size > 0) {
            init_worker_manager(worker_size);
        }

        if (argc >= 3) {
            int n = std::stoi(std::string(argv[2]));
            orks::userthread::start_main_thread(fibo_main, &n);
        } else {
            orks::userthread::start_main_thread(fibo_main, nullptr);
        }

    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "caught unknown exception" << std::endl;
        return 1;
    }



}
