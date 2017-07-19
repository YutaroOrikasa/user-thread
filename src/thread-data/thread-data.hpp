#pragma once

#include "../stackallocators.hpp"
#include "../mysetjmp.h"
namespace orks {
namespace userthread {
namespace detail {
namespace baddesign {


enum class ThreadState {
    running, ended, before_launch
};

class ThreadData {
    using Context = ThreadData*;

#ifdef ORKS_USERTHREAD_STACK_ALLOCATOR
    using StackAllocator = ORKS_USERTHREAD_STACK_ALLOCATOR;
#else
    using StackAllocator = SimpleStackAllocator;
#endif

    using Stack = StackAllocator::Stack;

public:
    context env;
    ThreadState state = ThreadState::before_launch;

    ThreadData* pass_on_longjmp = 0;
    void* transferred_data = nullptr;

private:
    Context(*func)(void* arg, Context prev);
    void* arg;
    Stack stack_frame;

#ifdef USE_SPLITSTACKS
    splitstack_context splitstack_context_;
#endif

public:

    template <typename Fn>
    ThreadData(Fn fn)
        : ThreadData((Context(*)(void*, Context)) & (exec_thread_delete<Fn>),
                     (void*)(new Fn(std::move(fn)))) {

    }

    ThreadData(Context(*func)(void* arg, Context prev), void* arg)
        : func(func)
        , arg(arg)
        , stack_frame() {

    }

    Context call_func(Context prev) {
        return func(arg, prev);
    }

    void restore_extra_context() {

    }


    void save_extra_context() {

    }


    void initialize_extra_context_at_entry_point() {

    }

    char* get_stack() {
        return stack_frame.stack.get();
    }

    std::size_t get_stack_size() {
        return stack_frame.size;
    }


    // non copyable
    ThreadData(const ThreadData&) = delete;
    ThreadData(ThreadData&&) = delete;


    // this function is public
    // this is bad
    template <typename Fn>
    static ThreadData* create(Fn fn) {
        Stack stack = StackAllocator::allocate();
        assert(stack.size != 0);
        auto th = new(stack.stack.get()) ThreadData(fn);
        th->stack_frame = std::move(stack);
        assert(th->get_stack_size() != 0);
        return th;

    }

    // this function is public
    // this is bad
    static void destroy(ThreadData& t) {

        Stack stack = std::move(t.stack_frame);
        t.~ThreadData();
    }

private:
    template<typename Fn>
    static Context exec_thread_delete(void* func_obj, Context t) {
        Context r = (*static_cast<Fn*>(func_obj))(t);
        delete static_cast<Fn*>(func_obj);
        return r;
    }


};

}
}
}
}