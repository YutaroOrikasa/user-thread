
#ifndef USER_THREAD_SPLITSTACKAPI_H
#define USER_THREAD_SPLITSTACKAPI_H

#include <stdlib.h>

enum __splitstack_context_offsets {
    MORESTACK_SEGMENTS = 0,
    CURRENT_SEGMENT = 1,
    CURRENT_STACK = 2,
    STACK_GUARD = 3,
    INITIAL_SP = 4,
    INITIAL_SP_LEN = 5,
    BLOCK_SIGNALS = 6,

    NUMBER_OFFSETS = 10
};

#ifdef __cplusplus
extern "C" {
#endif

extern void* __morestack_get_guard(void)
__attribute__((no_split_stack, visibility("hidden")));
//
//extern void __morestack_set_guard(void*)
//__attribute__((no_split_stack, visibility("hidden")));
//
extern void* __morestack_make_guard(void*, size_t)
__attribute__((no_split_stack, visibility("hidden")));

extern void __stack_split_initialize(void)
__attribute__((visibility("hidden")));


typedef void* splitstack_context[NUMBER_OFFSETS];
void __splitstack_getcontext(splitstack_context);
void __splitstack_setcontext(splitstack_context);
void*
__splitstack_makecontext(size_t stack_size, splitstack_context,
                         size_t* size);
void __splitstack_releasecontext(splitstack_context);
void*
__splitstack_resetcontext(splitstack_context, size_t* size);




#ifdef __cplusplus
}
#endif

#endif //USER_THREAD_SPLITSTACKAPI_H
