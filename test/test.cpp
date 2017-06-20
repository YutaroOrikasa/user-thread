#include <thread>
#include <atomic>
#include "gtest/gtest.h"
#include "user-thread.hpp"

using namespace orks::userthread;


namespace {
struct Args {
    WorkerManager& wm;
    const int thread_size;
    std::atomic_int& counter;
    std::atomic_int& alive_thread_counter;
};
void child_thread_for_test(void* arg) {
    printf("child thread\n");
    Args& args = *reinterpret_cast<Args*>(arg);

    ++args.counter;

    --args.alive_thread_counter;
}
void main_thread_for_test(void* arg) {
    printf("main thread\n");
    Args& args = *reinterpret_cast<Args*>(arg);
    for (int i = 0; i < args.thread_size; ++i) {
        args.wm.start_thread(child_thread_for_test, &args);
    }
    while (args.alive_thread_counter != 0) {
        args.wm.scheduling_yield();
    }
}

void child_thread_for_test_yield(void* arg) {
    printf("child thread\n");
    Args& args = *reinterpret_cast<Args*>(arg);

    ++args.counter;
    args.wm.scheduling_yield();
    ++args.counter;

    --args.alive_thread_counter;

}
void main_thread_for_test_yield(void* arg) {
    printf("main thread\n");
    Args& args = *reinterpret_cast<Args*>(arg);
    for (int i = 0; i < args.thread_size; ++i) {
        args.wm.start_thread(child_thread_for_test_yield, &args);
    }
    while (args.alive_thread_counter != 0) {
        args.wm.scheduling_yield();
    }
}

}

TEST(WorkerManager, Test) {

    WorkerManager wm { 4 };
    std::atomic_int counter { 0 };
    int thread_size = 8;
    std::atomic_int alive_thread_counter {thread_size};
    Args args = { wm, thread_size, counter, alive_thread_counter };
    wm.start_main_thread(main_thread_for_test, &args);
    ASSERT_EQ(thread_size, counter);
}

TEST(WorkerManager, TestYield) {

    WorkerManager wm { 4 };
    std::atomic_int counter { 0 };
    std::atomic_int finished_thread_counter {0};
    int thread_size = 8;
    std::atomic_int alive_thread_counter {thread_size};
    Args args = { wm, thread_size, counter, alive_thread_counter };
    wm.start_main_thread(main_thread_for_test_yield, &args);
    ASSERT_EQ(thread_size * 2, counter);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
