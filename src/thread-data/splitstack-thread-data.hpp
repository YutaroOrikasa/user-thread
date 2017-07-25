#pragma once

#include <cassert>
#include <user-thread-debug.hpp>

#include "thread-data.hpp"
#include "../stack-address-tools.hpp"
#include "../splitstackapi.h"
#include "../mysetjmp.h"

namespace orks {
namespace userthread {
namespace detail {
namespace baddesign {
namespace splitstack {

using namespace stacktool;

class ThreadData {
    using Context = ThreadData*;
    struct SplitstackContext {
        splitstack_context ctx;
    };


public:
    context env;
    ThreadState state = ThreadState::before_launch;

    ThreadData* pass_on_longjmp = 0;
    void* transferred_data = nullptr;

private:
    Context(*func)(void* arg, Context prev);
    void* arg;

    SplitstackContext splitstack_context_;
    void* stack = nullptr;
    std::size_t stack_size = 0;

public:

    template <typename Fn>
    ThreadData(Fn fn)
        : ThreadData((Context(*)(void*, Context)) & (exec_thread_delete<Fn>),
                     (void*)(new Fn(std::move(fn)))) {

    }

    ThreadData(Context(*func)(void* arg, Context prev), void* arg)
        : func(func)
        , arg(arg) {

    }

    Context call_func(Context prev) {
        return func(arg, prev);
    }

    // always_inline for no split stack
    __attribute__((always_inline))
    void restore_extra_context() {
        __splitstack_setcontext(splitstack_context_.ctx);
    }

    // always_inline for no split stack
    __attribute__((always_inline))
    void save_extra_context() {
        __splitstack_getcontext(splitstack_context_.ctx);
    }

    // always_inline for no split stack
    __attribute__((always_inline))
    void initialize_extra_context_at_entry_point() {
        __splitstack_setcontext(splitstack_context_.ctx);

        void* bottom = get_stack() + get_stack_size();
        void* split_stacks_boundary = __morestack_get_guard();

        debug::printf("stack top of new thread: %p, stack size of new thread: 0x%lx\n", get_stack(),
                      static_cast<unsigned long>(get_stack_size()));
        debug::printf("stack bottom of new thread: %p, stack boundary of new thread: %p\n", bottom, split_stacks_boundary);
        assert(more_forward_than(split_stacks_boundary, bottom));
    }

    char* get_stack() {
        return static_cast<char*>(stack);
    }

    std::size_t get_stack_size() {
        return stack_size;
    }


    // non copyable
    ThreadData(const ThreadData&) = delete;
    ThreadData(ThreadData&&) = delete;


    // this function is public
    // this is bad
    template <typename Fn>
    static ThreadData* create(Fn fn) {
        SplitstackContext ssctx;
        std::size_t size;
        void* stack = __splitstack_makecontext(SimpleStackAllocator::stack_size, ssctx.ctx, &size);
        assert(size != 0);
        auto th = new(stack) ThreadData(fn);
        th->splitstack_context_ = ssctx;
        th->stack = stack;
        th->stack_size = size;
        assert(th->get_stack_size() != 0);
        return th;

    }

    // this function is public
    // this is bad
    static void destroy(ThreadData& t) {
        SplitstackContext ssctx = t.splitstack_context_;
        t.~ThreadData();
        __splitstack_releasecontext(ssctx.ctx);

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
}