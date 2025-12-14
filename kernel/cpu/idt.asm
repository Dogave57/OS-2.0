global load_idt
section .text
load_idt:
cli
lidt [rcx]
sti
ret
