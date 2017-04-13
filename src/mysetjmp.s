        .text
        .globl mysetjmp
mysetjmp:
	movq %rbx, 0(%rdi)
	movq %rbp, 16(%rdi)
	movq %r12, 24(%rdi)
	movq %r13, 32(%rdi)
	movq %r14, 40(%rdi)
	movq %r15, 48(%rdi)

        popq %rcx
	movq %rsp, 8(%rdi)
	movq %rcx, 56(%rdi)
	pushq %rcx

        mov $0, %rax
	ret

        .text
        .globl mylongjmp
mylongjmp:
	movq 0(%rdi), %rbx
	movq 8(%rdi), %rsp
	movq 16(%rdi), %rbp
	movq 24(%rdi), %r12
	movq 32(%rdi), %r13
	movq 40(%rdi), %r14
	movq 48(%rdi), %r15

        mov $1, %rax
	jmp *56(%rdi)  # return
        ret
