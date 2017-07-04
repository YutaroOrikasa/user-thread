#ifndef USER_THREAD_USER_THREAD_DEBUG_HPP_H
#define USER_THREAD_USER_THREAD_DEBUG_HPP_H

#include <cstdio>
#include <iostream>
#include <utility>
#include <thread>

#include "config.h"
#include "../src/user-thread-internal.hpp"

namespace orks {
namespace userthread {
namespace detail {
const std::string& get_worker_name_of_this_native_thread();
}
}
}

namespace orks {
namespace userthread {

namespace detail {
namespace debug {

extern std::mutex debug_out_mutex;

class OutImpl {
    std::unique_lock<std::mutex> lock;
public:
    explicit OutImpl(std::unique_lock<std::mutex>&& unique_lock) :
        lock(std::move(unique_lock)) {
    }

    template <typename Rhs>
    OutImpl& operator<<(Rhs&& rhs) {
        std::cerr << std::forward<Rhs>(rhs);
        return *this;
    }

    /*
    * for (*this) << std::endl
    */
    auto& operator<<(std::ostream & (*pf)(std::ostream&)) {
        std::cerr << pf;
        return *this;
    }

    template <typename ... Args>
    int printf(const char* fmt, Args&& ...args) {
        return std::fprintf(stderr, fmt, std::forward<Args>(args)...);
    }

    void print_thread_name() {
        (*this) << get_worker_name_of_this_native_thread() << ": ";
    }
};

class Out {
public:

    /*
     * if ORKS_USERTHREAD_DEBUG_OUTPUT is define, output rhs to std::cerr
     * else, do nothing
     */
    template <typename Rhs>
    auto operator<<(Rhs&& rhs) {

#ifdef ORKS_USERTHREAD_DEBUG_OUTPUT
        OutImpl out {std::unique_lock<std::mutex>(debug_out_mutex)};
        out.print_thread_name();
        out << std::forward<Rhs>(rhs);
        return std::move(out);
#else
        // (*this) << "brabrabra"  will do nothing.
        return *this;
#endif
    }

    /*
     * for (*this) << std::endl
     */
    auto operator<<(std::ostream & (*pf)(std::ostream&)) {
#ifdef ORKS_USERTHREAD_DEBUG_OUTPUT
        OutImpl out {std::unique_lock<std::mutex>(debug_out_mutex)};
        out.print_thread_name();
        out << pf;
        return std::move(out);
#else
        return *this;
#endif
    }
};

namespace {
Out out;
}

template <typename ... Args>
int printf(const char* fmt, Args&& ...args) {
#ifdef ORKS_USERTHREAD_DEBUG_OUTPUT
    OutImpl out {std::unique_lock<std::mutex>(debug_out_mutex)};
    out.print_thread_name();
    return out.printf(fmt, std::forward<Args>(args)...);
#else
    return 0;
#endif
}

// suppress gcc warning: -Wformat-security
inline
int printf(const char* str) {
    return printf("%s", str);
}
}

}
}
}
#endif //USER_THREAD_USER_THREAD_DEBUG_HPP_H
