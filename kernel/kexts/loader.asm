global kext_bootstrap
section .text
kext_bootstrap:
mov rbp, rsp
jmp rcx
leave
ret
