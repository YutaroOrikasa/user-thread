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
    running, ended, before_launch, stop
};

class ThreadData {

#ifdef ORKS_USERTHREAD_STACK_ALLOCATOR
    using StackAllocator = ORKS_USERTHREAD_STACK_ALLOCATOR;
#else
    using StackAllocator = SimpleStackAllocator;
#endif

    using Stack = StackAllocator::Stack;

public:
    ThreadData& (*func)(void* arg, ThreadData& prev);
    void* arg;
    const Stack stack_frame;
    context env;
    ThreadState state = ThreadState::before_launch;

    ThreadData* pass_on_longjmp = 0;

#ifdef USE_SPLITSTACKS
    splitstack_context splitstack_context_;
#endif

public:

    template <typename Fn>
    ThreadData(Fn fn)
        : ThreadData((ThreadData & (*)(void*, ThreadData&)) & (exec_thread_delete<Fn>),
                     (void*)(new Fn(std::move(fn)))) {
//        auto a = exec_thread_delete<Fn>;
//        int i = a;
    }

    ThreadData(ThreadData & (*func)(void* arg, ThreadData& prev), void* arg)
        : func(func)
        , arg(arg)
        , stack_frame(StackAllocator::allocate()) {

        assert(this->stack_frame.stack.get() != 0);

    }



    // non copyable
    ThreadData(const ThreadData&) = delete;
    ThreadData(ThreadData&&) = delete;


    template<typename Fn>
    static ThreadData& exec_thread_delete(void* func_obj, ThreadData& t) {
        ThreadData& r = (*static_cast<Fn*>(func_obj))(t);
        delete static_cast<Fn*>(func_obj);
        return r;
    }


};

template<class Worker>
struct BadDesignContextTraitsImpl {
    using Context = ThreadData;
    friend Worker;
//    static Context& switch_context_(Context& next) {
//
//        debug::printf("jump to ThreadData* %p\n", &next);
//        auto& prev = switch_context_impl(next);
//
//        call_after_context_switch(prev);
//        return prev;
//
//    }

    static Context& switch_context(Context& next_thread) {

        ThreadData* volatile current_thread = get_current_thread();

        if (current_thread == nullptr) {
            current_thread = new ThreadData(nullptr, nullptr);
            current_thread->state = ThreadState::running;
        }

        if (current_thread->state != ThreadState::ended) {
            current_thread->state = ThreadState::stop;
        }
        debug::printf("current thread %p, ended: %d\n", current_thread, current_thread->state == ThreadState::ended);
        debug::printf("execute thread %p, stack frame is %p\n", &next_thread, next_thread.stack_frame.stack.get());

#ifdef USE_SPLITSTACKS
        __splitstack_getcontext(current_thread->splitstack_context_);
#endif

        ThreadData* previous_thread = 0;
        if (next_thread.state == ThreadState::before_launch) {
            debug::printf("launch user thread!\n");

            previous_thread = &context_switch_new_context(*current_thread, next_thread);

        } else if (next_thread.state == ThreadState::stop) {
            debug::printf("resume user thread %p!\n", &next_thread);
            next_thread.state = ThreadState::running;
#ifdef USE_SPLITSTACKS
            __splitstack_setcontext(next_thread.splitstack_context_);
#endif
            previous_thread = &context_switch(*current_thread, next_thread);

        } else {
            debug::out << "next_thread " << &next_thread << " invalid state: " << static_cast<int>(next_thread.state)
                       << "\n";
            const auto NEVER_COME_HERE = false;
            assert(NEVER_COME_HERE);

        }

        set_current_thread(*current_thread);

        return *previous_thread;

    }

//    static Context& make_context() {
//
//    }

    static bool is_finished(Context& ctx) {
        return ctx.state == ThreadState::ended;
    }
    static void destroy_context(Context& ctx) {
        delete &ctx;
    }

private:

    static void set_current_thread(ThreadData& t) {
        Worker::get_worker_of_this_native_thread().current_thread = &t;
    }

    static ThreadData* get_current_thread() {
        auto t = Worker::get_worker_of_this_native_thread().current_thread;
        if (t == nullptr) {
            return Worker::get_worker_of_this_native_thread().worker_thread_context;
        }
        return t;
    }


    // always_inline for no split stack
    __attribute__((always_inline))
    static ThreadData& context_switch(ThreadData& from, ThreadData& to) {
        if (mysetjmp(from.env)) {
            return *from.pass_on_longjmp;
        }
        to.pass_on_longjmp = &from;
        mylongjmp(to.env);

    }

    // always_inline for no split stack
    __attribute__((always_inline))
    static ThreadData& context_switch_new_context(ThreadData& from, ThreadData& new_ctx) {

        if (mysetjmp(from.env)) {
            return *from.pass_on_longjmp;
        }
        new_ctx.pass_on_longjmp = &from;
        char* stack_frame = new_ctx.stack_frame.stack.get();
        call_with_alt_stack_arg3(stack_frame, new_ctx.stack_frame.size, reinterpret_cast<void*>(entry_thread),
                                 &new_ctx, nullptr, nullptr);

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


    // TODO: move this to switch_context_impl
    // do this on prev context before setting new stack
    set_current_thread(thread_data);


    debug::printf("start thread in new stack frame\n");
    debug::out << std::endl;


    thread_data.state = ThreadState::running;

    ThreadData& next = thread_data.func(thread_data.arg, *thread_data.pass_on_longjmp);

    debug::printf("end thread\n");
    thread_data.state = ThreadState::ended;
    debug::printf("end: %p\n", &thread_data);

    switch_context(next);
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
