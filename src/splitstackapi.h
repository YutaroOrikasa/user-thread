
#ifndef USER_THREAD_SPLITSTACKAPI_H
#define USER_THREAD_SPLITSTACKAPI_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif
extern void* __morestack_get_guard(void)
__attribute__((no_split_stack, visibility("hidden")));

extern void __morestack_set_guard(void*)
__attribute__((no_split_stack, visibility("hidden")));

extern void* __morestack_make_guard(void*, size_t)
__attribute__((no_split_stack, visibility("hidden")));
#ifdef __cplusplus
}
#endif

#endif //USER_THREAD_SPLITSTACKAPI_H
