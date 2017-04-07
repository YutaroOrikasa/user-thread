#include <thread>
#include <atomic>
#include "gtest/gtest.h"
#include "user-thread.hpp"

using namespace orks::userthread;

TEST(WorkQueue, FinishAtAllWorkerPop) {
    using namespace orks::userthread::detail;

    WorkQueue wq { 4 };
    auto worker = [&]() {
        while (wq.pop() != nullptr);
    };

    std::thread th0 { worker };
    std::thread th1 { worker };
    std::thread th2 { worker };
    std::thread th3 { worker };
    ThreadData td { 0, 0, 0 };
    wq.push(td);

    th0.join();
    th1.join();
    th2.join();
    th3.join();


}

namespace {
struct Args {
    WorkerManager& wm;
    const int thread_size;
    std::atomic_int& counter;
};
void child_thread_for_test(void* arg) {
    printf("child thread\n");
    std::atomic_int& counter = *reinterpret_cast<std::atomic_int*>(arg);
    ++counter;
}
void main_thread_for_test(void* arg) {
    printf("main thread\n");
    Args& args = *reinterpret_cast<Args*>(arg);
    for (int i = 0; i < args.thread_size; ++i) {
        args.wm.start_thread(child_thread_for_test, &args.counter);
    }
}

void child_thread_for_test_yield(void* arg) {
    printf("child thread\n");
    Args& args = *reinterpret_cast<Args*>(arg);

    ++args.counter;
    args.wm.scheduling_yield();
    ++args.counter;

}
void main_thread_for_test_yield(void* arg) {
    printf("main thread\n");
    Args& args = *reinterpret_cast<Args*>(arg);
    for (int i = 0; i < args.thread_size; ++i) {
        args.wm.start_thread(child_thread_for_test_yield, &args);
    }
}

}

TEST(WorkerManager, Test) {

    WorkerManager wm { 4 };
    std::atomic_int counter { 0 };
    int thread_size = 8;
    Args args = { wm, thread_size, counter };
    wm.start_main_thread(main_thread_for_test, &args);
    ASSERT_EQ(thread_size, counter);
}

TEST(WorkerManager, TestYield) {

    WorkerManager wm { 4 };
    std::atomic_int counter { 0 };
    int thread_size = 8;
    Args args = { wm, thread_size, counter };
    wm.start_main_thread(main_thread_for_test_yield, &args);
    ASSERT_EQ(thread_size * 2, counter);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
