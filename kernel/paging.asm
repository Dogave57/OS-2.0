global load_pt
section .text
extern reload_cs
load_pt:
;mov dword cr3, rcx
cli
call reload_cs
sti
xor rax, rax
ret
