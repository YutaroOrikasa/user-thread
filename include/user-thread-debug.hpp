#ifndef USER_THREAD_USER_THREAD_DEBUG_HPP_H
#define USER_THREAD_USER_THREAD_DEBUG_HPP_H

#include <cstdio>
#include <iostream>
#include <utility>
#include <thread>

namespace orks {
namespace userthread {
namespace detail {
namespace debug {
class Out {
public:
    template <typename Rhs>
    auto& operator<<(Rhs&& rhs) {
        std::cerr << "thread at " << std::this_thread::get_id() << ": " << std::forward<Rhs>(rhs);
        return std::cerr;
    }
};

namespace {
Out out;
}

template <typename ... Args>
int printf(const char* fmt, Args&& ...args) {
    out << "";
    return std::fprintf(stderr, fmt, std::forward<Args>(args)...);
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
