#ifndef USER_THREAD_ALLOCATORS_HPP_HPP
#define USER_THREAD_ALLOCATORS_HPP_HPP

#include <cstdlib>
#include <memory>
#include <utility>
#include <boost/utility/value_init.hpp>

namespace orks {
namespace userthread {
namespace detail {

template <typename T>
struct reinitialize_on_move : boost::initialized<T> {
    using base_t = boost::initialized<T>;
    using base_t::initialized;
    reinitialize_on_move() = default;
    reinitialize_on_move(T t)
        : base_t(t) {

    }
    reinitialize_on_move(reinitialize_on_move&& other) {
        *this = std::move(other);
    }

    reinitialize_on_move& operator=(reinitialize_on_move&& other) noexcept {
        if (this != &other) {
            this->data() = std::exchange(other.data(), boost::initialized_value);
        }
        return  *this;
    }

};

struct SimpleStackAllocator {
    static constexpr std::size_t stack_size = 0x4000;

    struct Deleter {
        void operator()(char* p) {
            SimpleStackAllocator::deallocate(p);
        }
    };

    struct Stack {
        std::unique_ptr<char[], SimpleStackAllocator::Deleter> stack;
        reinitialize_on_move<std::size_t> size;

    };

    static Stack allocate() {

        return Stack{
            std::unique_ptr < char[], SimpleStackAllocator::Deleter>(new char[stack_size]),
            stack_size
        };
    }

    static void deallocate(char* p) {
        delete[] p;
    }

};
}
}
}
#endif //USER_THREAD_ALLOCATORS_HPP_HPP
