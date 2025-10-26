global load_pt
global get_pt
section .text
extern reload_cs
load_pt:
mov dword cr3, rcx
cli
call reload_cs
sti
xor rax, rax
ret
get_pt:
xor rax, rax
mov rax, cr3
ret
