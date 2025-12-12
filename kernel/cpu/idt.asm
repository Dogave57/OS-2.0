global load_idt
section .text
load_idt:
cli
lidt [rdi]
sti
ret
