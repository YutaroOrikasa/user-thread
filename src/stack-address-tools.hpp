#ifndef USER_THREAD_STACK_ADDRESS_TOOL_HPP
#define USER_THREAD_STACK_ADDRESS_TOOL_HPP

namespace orks {
namespace userthread {
namespace detail {
namespace stacktool {

constexpr bool stack_grows_downword = false;


inline bool more_forward_than(void* forward, void* backward) {
    if (stack_grows_downword) {
        return forward > backward;
    } else {
        return forward < backward;
    }

}

inline bool more_forward_equal(void* forward, void* backward) {
    if (stack_grows_downword) {
        return forward >= backward;
    } else {
        return forward <= backward;
    }

}
}
}
}
}

#endif //USER_THREAD_STACK_ADDRESS_TOOL_HPP
