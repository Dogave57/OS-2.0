global load_pml4
global get_pml4
global flush_tlb
global flush_full_tlb
section .text
extern reload_cs
load_pml4:
mov cr3, rcx
xor rax, rax
ret
get_pml4:
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
