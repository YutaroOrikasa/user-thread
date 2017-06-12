        .text
        .globl orks_private_call_with_alt_stack_arg3_impl
orks_private_call_with_alt_stack_arg3_impl:
	movq %rcx, %rsp
	callq *%r8
    ud2
	ret
