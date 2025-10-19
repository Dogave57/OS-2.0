global load_gdt
section .text
reload_cs:
mov ax, 10h
mov ds, ax
mov es, ax
mov gs, ax
mov fs, ax
mov ss, ax
mov qword rax, [rsp]
push qword 0x08
push qword rax
retfq
load_gdt:
cli
lgdt [rcx]
call reload_cs
sti
ret
load_gdt_fail:
cli
hlt
b:
jmp b
