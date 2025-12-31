section .text
global get_rflags
get_rflags:
pushfq
mov qword rax, [rsp]
mov qword [rsp], rax
popfq
ret
