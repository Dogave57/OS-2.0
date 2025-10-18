extern kmain
section .text
global kernel_stub
kernel_stub:
;mov rsp, rcx
sub rsp, 32
call kmain
add rsp, 32
cli
hlt
ret
