#ifndef USER_THREAD_ALLOCATORS_HPP_HPP
#define USER_THREAD_ALLOCATORS_HPP_HPP

#include <cstdlib>
#include <memory>
namespace orks {
namespace userthread {
namespace detail {

struct SimpleStackAllocator {
    static constexpr size_t stack_size = 0xffff;

    struct Deleter {
        void operator()(char* p) {
            SimpleStackAllocator::deallocate(p);
        }
    };

    struct Stack {
        std::unique_ptr<char[], SimpleStackAllocator::Deleter> stack;
        std::size_t size = stack_size;
    };

    static Stack allocate() {
        return Stack{
            std::unique_ptr < char[], SimpleStackAllocator::Deleter>(new char[stack_size])
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
