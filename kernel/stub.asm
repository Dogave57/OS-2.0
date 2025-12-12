extern kmain
section .text
global kernel_stub
kernel_stub:
mov rdi, rcx
mov rsi, rdx
mov rsp, rdi
sub rsp, 32
jmp kmain
add rsp, 32
cli
hlt
ret
