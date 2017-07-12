#ifndef USER_THREAD_CONTEXTTRAITS_HPP
#define USER_THREAD_CONTEXTTRAITS_HPP

#include "mysetjmp.h"
#include "user-thread-debug.hpp"
#include "workqueue.hpp"
#include "stackallocators.hpp"
#include "splitstackapi.h"
#include "stack-address-tools.hpp"


#include "call_with_alt_stack_arg3.h"

#include "config.h"


namespace orks {
namespace userthread {
namespace detail {

class Worker;

using namespace stacktool;


}
}
}

namespace orks {
namespace userthread {
namespace detail {
namespace baddesign {

inline
void call_with_alt_stack_arg3(char* altstack, std::size_t altstack_size, void* func, void* arg1, void* arg2, void* arg3) __attribute__((no_split_stack));

inline
void call_with_alt_stack_arg3(char* altstack, std::size_t altstack_size, void* func, void* arg1, void* arg2, void* arg3) {
    void* stack_base = altstack + altstack_size;
#ifndef USE_SPLITSTACKS
    // stack boundary was already changed by caller.
    // debug output will make memory destorying.
    debug::printf("func %p\n", func);
#endif

    orks_private_call_with_alt_stack_arg3_impl(arg1, arg2, arg3, stack_base, func);
}


enum class ThreadState {
    running, ended, before_launch
};

class ThreadData;
using Context = ThreadData*;

class ThreadData {

#ifdef ORKS_USERTHREAD_STACK_ALLOCATOR
    using StackAllocator = ORKS_USERTHREAD_STACK_ALLOCATOR;
#else
    using StackAllocator = SimpleStackAllocator;
#endif

    using Stack = StackAllocator::Stack;

public:
    Context(*func)(void* arg, Context prev);
    void* arg;
    Stack stack_frame;
    context env;
    ThreadState state = ThreadState::before_launch;

    ThreadData* pass_on_longjmp = 0;

#ifdef USE_SPLITSTACKS
    splitstack_context splitstack_context_;
#endif

public:

    template <typename Fn>
    ThreadData(Fn fn)
        : ThreadData((Context(*)(void*, Context)) & (exec_thread_delete<Fn>),
                     (void*)(new Fn(std::move(fn)))) {
//        auto a = exec_thread_delete<Fn>;
//        int i = a;
    }

    ThreadData(Context(*func)(void* arg, Context prev), void* arg)
        : func(func)
        , arg(arg)
        , stack_frame() {

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
        assert(th->stack_frame.size != 0);
        return th;

    }

    // this function is public
    // this is bad
    static void destroy(ThreadData& t) {
#ifdef USE_SPLITSTACKS
        __splitstack_releasecontext(t.splitstack_context_);
#endif
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

template<class Worker>
struct BadDesignContextTraitsImpl {
    using Context = ThreadData*;
    friend Worker;
//    static Work& switch_context_(Work& next) {
//
//        debug::printf("jump to Work* %p\n", &next);
//        auto& prev = switch_context_impl(next);
//
//        call_after_context_switch(prev);
//        return prev;
//
//    }

    template <typename Fn>
    static Context make_context(Fn fn) {
        return ThreadData::create(std::move(fn));
    }

    static Context switch_context(Context next_thread) {
        return &switch_context_impl(*next_thread);
    }

    static bool is_finished(Context ctx) {
        return ctx->state == ThreadState::ended;
    }

    static void destroy_context(Context ctx) {
        ThreadData::destroy((*ctx));
    }

private:
    static ThreadData& switch_context_impl(ThreadData& next_thread, ThreadData* finished_thread = nullptr) {

        ThreadData current_thread_ {nullptr, nullptr};
        ThreadData* current_thread;

        if (finished_thread == nullptr) {
            current_thread = &current_thread_;
            debug::printf("save current thread at %p\n", current_thread);

            current_thread->state = ThreadState::running;

        } else {
            current_thread = finished_thread;
            debug::printf("current thread %p is finished\n", current_thread);
        }


        ThreadData* previous_thread = 0;
        if (next_thread.state == ThreadState::before_launch) {
            debug::printf("launch user thread!\n");

            previous_thread = &context_switch_new_context(*current_thread, next_thread);

        } else if (next_thread.state == ThreadState::running) {
            debug::printf("resume user thread %p!\n", &next_thread);

            previous_thread = &context_switch(*current_thread, next_thread);

        } else {
            debug::out << "next_thread " << &next_thread << " invalid state: " << static_cast<int>(next_thread.state)
                       << "\n";
            const auto NEVER_COME_HERE = false;
            assert(NEVER_COME_HERE);

        }

        return *previous_thread;

    }


    // always_inline for no split stack
    __attribute__((always_inline))
    static ThreadData& context_switch(ThreadData& from, ThreadData& to) {

#ifdef USE_SPLITSTACKS
        __splitstack_getcontext(from.splitstack_context_);
#endif
        if (mysetjmp(from.env)) {
#ifdef USE_SPLITSTACKS
            __splitstack_setcontext(from.splitstack_context_);
#endif
            return *from.pass_on_longjmp;
        }
        to.pass_on_longjmp = &from;
        mylongjmp(to.env);

        const auto NEVER_COME_HERE = false;
        assert(NEVER_COME_HERE);


    }

    // always_inline for no split stack
    __attribute__((always_inline))
    static ThreadData& context_switch_new_context(ThreadData& from, ThreadData& new_ctx) {

#ifdef USE_SPLITSTACKS
        __splitstack_getcontext(from.splitstack_context_);
#endif
        if (mysetjmp(from.env)) {
#ifdef USE_SPLITSTACKS
            __splitstack_setcontext(from.splitstack_context_);
#endif
            return *from.pass_on_longjmp;
        }
        new_ctx.pass_on_longjmp = &from;
        assert(new_ctx.stack_frame.stack.get() != 0);
        assert(new_ctx.stack_frame.size != 0);
        char* stack_frame = new_ctx.stack_frame.stack.get();
        call_with_alt_stack_arg3(stack_frame, new_ctx.stack_frame.size, reinterpret_cast<void*>(entry_thread),
                                 &new_ctx, nullptr, nullptr);

        const auto NEVER_COME_HERE = false;
        assert(NEVER_COME_HERE);

    }

    __attribute__((no_split_stack))
    static void entry_thread(ThreadData& thread_data);

};


template<class Worker>
void BadDesignContextTraitsImpl<Worker>::entry_thread(ThreadData& thread_data) {

#ifdef USE_SPLITSTACKS
    __stack_split_initialize();

    void* bottom = thread_data.stack_frame.stack.get() + thread_data.stack_frame.size;
    void* split_stacks_boundary = __morestack_get_guard();

    debug::printf("stack top of new thread: %p, stack size of new thread: 0x%lx\n", thread_data.stack_frame.stack.get(),
                  static_cast<unsigned long>(thread_data.stack_frame.size));
    debug::printf("stack bottom of new thread: %p, stack boundary of new thread: %p\n", bottom, split_stacks_boundary);
    assert(more_forward_than(split_stacks_boundary, bottom));
#endif

    debug::printf("start thread in new stack frame\n");
    debug::out << std::endl;


    thread_data.state = ThreadState::running;

    Context next = thread_data.func(thread_data.arg, thread_data.pass_on_longjmp);

    debug::printf("end thread\n");
    thread_data.state = ThreadState::ended;
    debug::printf("end: %p\n", &thread_data);

    switch_context_impl(*next, &thread_data);
    // no return
    // this thread context will be deleted by next thread
}


}
}
}
}

namespace orks {
namespace userthread {
namespace detail {

using BadDesignContextTraits = baddesign::BadDesignContextTraitsImpl<Worker>;

}
}
}
#endif //USER_THREAD_CONTEXTTRAITS_HPP
