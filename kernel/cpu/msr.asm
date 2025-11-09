section .text
global read_msr
global write_msr
read_msr:
xor rdx, rdx
xor rax, rax
rdmsr
shl rdx, 32
or rax, rdx
ret
write_msr:
mov eax, edx
shr rdx, 32
wrmsr
xor rax, rax
ret
