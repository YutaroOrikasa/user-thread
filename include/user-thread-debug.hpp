#ifndef USER_THREAD_USER_THREAD_DEBUG_HPP_H
#define USER_THREAD_USER_THREAD_DEBUG_HPP_H

#include <cstdio>
#include <iostream>
#include <utility>
#include <thread>

#include "config.h"

namespace orks {
namespace userthread {

namespace detail {
namespace debug {

class Out {
public:

    /*
     * if ORKS_USERTHREAD_DEBUG_OUTBUT is define, output rhs to std::cerr
     * else, do nothing
     */
    template <typename Rhs>
    auto& operator<<(Rhs&& rhs) {

#ifdef ORKS_USERTHREAD_DEBUG_OUTBUT
        std::cerr << "thread at " << std::this_thread::get_id() << ": " << std::forward<Rhs>(rhs);
#endif

        return std::cerr;
    }
};

namespace {
Out out;
}

template <typename ... Args>
int printf(const char* fmt, Args&& ...args) {
#ifdef ORKS_USERTHREAD_DEBUG_OUTBUT
    out << "";
    return std::fprintf(stderr, fmt, std::forward<Args>(args)...);
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
