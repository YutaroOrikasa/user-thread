#ifndef MYSETJMP_H_
#define MYSETJMP_H_

struct context {
    uint64_t regs[8];
};

#ifdef __cplusplus
extern "C" {
#endif

uint64_t mysetjmp(context& ctx);
void mylongjmp(context& ctx);

#ifdef __cplusplus
}
#endif


#endif /* MYSETJMP_H_ */
