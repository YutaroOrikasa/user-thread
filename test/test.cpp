#include <thread>
#include <atomic>
#include "gtest/gtest.h"
#include "user-thread.hpp"

using namespace orks::userthread;


namespace {
struct TestData {
    WorkerManager& wm;
    const int thread_size = 8;
    std::atomic_int counter {0};
    std::atomic_int alive_thread_counter {thread_size};
};

void main_thread_for_test_main_thread(void* arg) {
    printf("main thread\n");
    (*static_cast<std::atomic_int*>(arg)) = 1;
}

void child_thread_for_test(void* arg) {
    printf("child thread\n");
    TestData& args = *reinterpret_cast<TestData*>(arg);

    ++args.counter;

    --args.alive_thread_counter;
}
void main_thread_for_test(void* arg) {
    printf("main thread\n");
    TestData& args = *reinterpret_cast<TestData*>(arg);
    for (int i = 0; i < args.thread_size; ++i) {
        args.wm.start_thread(child_thread_for_test, &args);
    }
    while (args.alive_thread_counter != 0) {
        args.wm.scheduling_yield();
    }
}

void child_thread_for_test_yield(void* arg) {
    printf("child thread\n");
    TestData& args = *reinterpret_cast<TestData*>(arg);

    ++args.counter;
    args.wm.scheduling_yield();
    printf("child thread resumed\n");
    ++args.counter;

    --args.alive_thread_counter;

}
void main_thread_for_test_yield(void* arg) {
    printf("main thread\n");
    TestData& args = *reinterpret_cast<TestData*>(arg);
    for (int i = 0; i < args.thread_size; ++i) {
        args.wm.start_thread(child_thread_for_test_yield, &args);
    }
    while (args.alive_thread_counter != 0) {
        printf("main thread yield\n");
        args.wm.scheduling_yield();
    }
}


void rec(unsigned int n) {
    volatile char c[0x1000] = {};
    static_cast<void>(c);
    if (n == 0) {
        return;
    }
    rec(n - 1);
}

void rec_thread(WorkerManager* wm, unsigned int n) {
    volatile char c[0x1000] = {};
    static_cast<void>(c);
    if (n == 0) {
        return;
    }
    detail::create_thread(*wm, rec_thread, wm, n - 1);
}

}

TEST(WorkerManager, TestMainThreadWith1Worker) {

    WorkerManager wm { 1 };
    std::atomic_int i {0};
    wm.start_main_thread(main_thread_for_test_main_thread, &i);
    ASSERT_EQ(i, 1);
}

TEST(WorkerManager, TestMainThread) {

    WorkerManager wm { 4 };
    std::atomic_int i {0};
    wm.start_main_thread(main_thread_for_test_main_thread, &i);
    ASSERT_EQ(i, 1);
}

TEST(WorkerManager, TestWith1Worker) {

    WorkerManager wm { 1 };
    TestData args {wm};
    wm.start_main_thread(main_thread_for_test, &args);
    ASSERT_EQ(args.thread_size, args.counter);
}

TEST(WorkerManager, TestYieldWith1Worker) {

    WorkerManager wm { 1 };
    TestData args {wm};
    wm.start_main_thread(main_thread_for_test_yield, &args);
    ASSERT_EQ(args.thread_size * 2, args.counter);
}


TEST(WorkerManager, Test) {

    WorkerManager wm { 4 };
    TestData args {wm};
    wm.start_main_thread(main_thread_for_test, &args);
    ASSERT_EQ(args.thread_size, args.counter);
}

TEST(WorkerManager, TestYield) {

    WorkerManager wm { 4 };
    TestData args {wm};
    wm.start_main_thread(main_thread_for_test_yield, &args);
    ASSERT_EQ(args.thread_size * 2, args.counter);
}

#ifdef USE_SPLITSTACKS
TEST(TestBigLocalArray, RecCall) {

    WorkerManager wm { 4 };
    detail::start_main_thread(wm, rec, 10);
}

TEST(TestBigLocalArray, RecThreadWith1Worker) {

    WorkerManager wm { 1 };
    detail::start_main_thread(wm, rec_thread, &wm, 10);
}

TEST(TestBigLocalArray, RecThread) {

    WorkerManager wm { 4 };
    detail::start_main_thread(wm, rec_thread, &wm, 10);
}

#endif

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
