#ifndef USER_THREAD_UTIL_HPP
#define USER_THREAD_UTIL_HPP

#include <mutex>

// internal helper
namespace orks {
namespace userthread {
namespace detail {
namespace util {

template<typename F>
class scope_exit {
    F f;
    bool execute;
public:
    explicit scope_exit(F disposer) :
        f(disposer), execute(true) {
    }

    scope_exit(scope_exit&& other) :
        f(other.f), execute(other.execute) {
        other.execute = false;
    }

    ~scope_exit() {
        if (execute) {
            try {
                f();
            } catch (...) {
            }
        }
    }

};

template<typename F>
auto make_scope_exit(F f) {
    return scope_exit<F>(f);
}

template<class Mutex>
auto make_unique_lock(Mutex& mutex) {
    return std::unique_lock<Mutex>(mutex);
}
}
}
}
}


#endif //USER_THREAD_UTIL_HPP
