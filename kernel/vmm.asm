global load_pt
global get_pt
global flush_tlb
global flush_full_tlb
section .text
extern reload_cs
load_pt:
cli
mov cr3, rcx
sti
xor rax, rax
ret
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
flush_tlb:
invlpg [rcx]
xor rax, rax
ret
flush_full_tlb:
mov rax, cr3
mov cr3, rax
xor rax, rax
ret
