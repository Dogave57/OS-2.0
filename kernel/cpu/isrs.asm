%macro pushaq 0
push rax
push rbx
push rcx
push rdx
push rsi
push rdi
push r8
push r9
push r10
push r11
push r12
push r13
push r14
push r15
%endmacro
%macro popaq 0
pop r15
pop r14
pop r13
pop r12
pop r11
pop r10
pop r9
pop r8
pop rdi
pop rsi
pop rdx
pop rcx
pop rbx
pop rax
%endmacro
exception_names:
de_name dw __?utf16?__('divide by zero'), 10, 0
db_name dw __?utf16?__('debug'), 10, 0
nmi_name dw __?utf16?__('non-maskable interrupt'), 10, 0
bp_name dw __?utf16?__('breakpoint'), 10, 0
of_name dw __?utf16?__('overflow'), 10, 0
br_name dw __?utf16?__('bound range exceeded'), 10, 0
ud_name dw __?utf16?__('invalid opcode'), 10, 0
nm_name dw __?utf16?__('device not available'), 10, 0
df_name dw __?utf16?__('double fault'), 10, 0
csor_name dw __?utf16?__('coprocessor segment overrun'), 10, 0
ts_name dw __?utf16?__('invalid tss'), 10, 0
np_name dw __?utf16?__('segment not present'), 10, 0
ss_name dw __?utf16?__('stack segment fault'), 10, 0
gp_name dw __?utf16?__('general protection fault'), 10, 0
pf_name dw __?utf16?__('page fault'), 10, 0
mf_name dw __?utf16?__('x87 floating point exception'), 10, 0
ac_name dw __?utf16?__('alignment check'), 10, 0
mc_name dw __?utf16?__('machine check'), 10, 0
xm_name dw __?utf16?__('simd floating point exception'), 10, 0
ve_name dw __?utf16?__('virtualization exception'), 10, 0
cp_name dw __?utf16?__('control protection exception'), 10, 0
hv_name dw __?utf16?__('hypervisor injection exception'), 10, 0
vc_name dw __?utf16?__('vmm communication exception'), 10, 0
sx_name dw __?utf16?__('security exception'), 10, 0
exception_name_map:
dq de_name;0
dq db_name;1
dq nmi_name;2
dq bp_name;3
dq of_name;4
dq br_name;5
dq ud_name;6
dq nm_name;7
dq df_name;8
dq csor_name;9
dq ts_name;10
dq np_name;11
dq ss_name;12
dq gp_name;13
dq pf_name;14
dq 0;reserved 15
dq mf_name;16
dq ac_name;17
dq mc_name;18
dq xm_name;19
dq ve_name;20
dq cp_name;21
dq 0;22-27 reserved
dq 0;23
dq 0;24
dq 0;25
dq 0;26
dq 0;27
dq hv_name;28
dq vc_name;29
dq sx_name;30
dq 0;reserved 31
saved_registers:
saved_reg_rip:
dq 0
saved_reg_rsp:
dq 0
saved_reg_rbp:
dq 0
section .bss
safe_stack_bottom:
resb 1024 
safe_stack_top:
resb 8
section .data
exceptionmsg:
dw __?utf16?__('exception 0x%x'), 13, 10, 0
regmsg:
dw __?utf16?__('rip: %p  '), __?utf16?__('rsp: %p  '), __?utf16?__('rbp: %p'), 13, 10, __?utf16?__('rax: 0x%x  '), __?utf16?__('rbx: 0x%x  '),__?utf16?__('rcx: 0x%x  '), 13, 10, __?utf16?__('rdx: 0x%x  '), __?utf16?__('cs: 0x%x  '), __?utf16?__('ds: 0x%x'), 13, 10, 0
section .text
global reset_timer
global get_time_ms
global set_time_ms
global default_isr
global pic_timer_isr
global timer_isr
global thermal_isr
global ps2_kbd_isr
global isr0
global isr1
global isr2
global isr3
global isr4
global isr5
global isr6
global isr7
global isr8
global isr10
global isr11
global isr12
global isr13
global isr14
global isr16
global isr17
global isr18
global isr19
global isr20
extern print
extern printf
extern clear
extern set_text_color
extern lapic_send_eoi
extern time_ms
extern thermal_state
extern putchar
extern ps2_keyboard_handler
exception_fg:
db 255, 255, 255
exception_bg:
db 0, 0, 0
deadly_exception:
pushaq
mov rcx, exception_fg
mov rdx, exception_bg
sub rsp, 32
call set_text_color
call clear
mov rdx, [rbp-8]
lea rcx, [rel exception_name_map]
imul rdx, rdx, 8
add rcx, rdx
mov rcx, [rcx]
call print
add rsp, 32
popaq
pushaq
mov rcx, exceptionmsg
mov rdx, [rbp-8]
sub rsp, 32
call printf
add rsp, 32
popaq
sub qword rsp, 16
mov word [rsp], cs
mov word [rsp+8], ds
push rdx
push rcx
push rbx
push rax
mov rcx, regmsg
mov rdx, [rel saved_reg_rip]
mov r8, [rel saved_reg_rsp]
mov r9, [rel saved_reg_rbp]
sub rsp, 32
call printf
add rsp, 32
b:
jmp b
hlt
ret
default_isr:
sti
iretq
msg dw __?utf16?__('keyboard'), 13, 10, 0
pic_timer_isr:
cli
pushaq
add qword [rel time_ms], 1
mov al, 20h
mov dx, 20h
out dx, al
popaq
sti
iretq
timer_isr:
cli
pushaq
mov rcx, msg
sub rsp, 32
;call print
add rsp, 32
add qword [rel time_ms], 1
call lapic_send_eoi
popaq
sti
iretq
thermal_isr:
cli
pushaq
call lapic_send_eoi
popaq
sti
iretq
ps2_kbd_isr:
cli
pushaq
call ps2_keyboard_handler
call lapic_send_eoi
popaq
sti
iretq
isr0:
cli
push rax
mov qword rax, [rsp+8]
mov qword [rel saved_reg_rip], rax
pop rax
mov qword [rel saved_reg_rsp], rsp
add qword [rel saved_reg_rsp], 24
mov qword [rel saved_reg_rbp], rbp
lea rsp, [rel safe_stack_top]
mov rbp, rsp
push qword 0
call deadly_exception
hlt
isr1:
cli
push rax
mov qword rax, [rsp+8]
mov qword [rel saved_reg_rip], rax
pop rax
mov qword [rel saved_reg_rsp], rsp
add qword [rel saved_reg_rsp], 24
mov qword [rel saved_reg_rbp], rbp
lea rsp, [rel safe_stack_top]
mov rbp, rsp
push qword 1
call deadly_exception
hlt
isr2:
cli
push rax
mov qword rax, [rsp+8]
mov qword [rel saved_reg_rip], rax
pop rax
mov qword [rel saved_reg_rsp], rsp
add qword [rel saved_reg_rsp], 24
mov qword [rel saved_reg_rbp], rbp
lea rsp, [rel safe_stack_top]
mov rbp, rsp
push qword 2
call deadly_exception
hlt
isr3:
cli
push rax
mov qword rax, [rsp+8]
mov qword [rel saved_reg_rip], rax
pop rax
mov qword [rel saved_reg_rsp], rsp
add qword [rel saved_reg_rsp], 24
mov qword [rel saved_reg_rbp], rbp
lea rsp, [rel safe_stack_top]
mov rbp, rsp
push qword 3
call deadly_exception
hlt
isr4:
cli
push rax
mov qword rax, [rsp+8]
mov qword [rel saved_reg_rip], rax
pop rax
mov qword [rel saved_reg_rsp], rsp
add qword [rel saved_reg_rsp], 24
mov qword [rel saved_reg_rbp], rbp
lea rsp, [rel safe_stack_top]
mov rbp, rsp
push qword 4
call deadly_exception
hlt
isr5:
cli
push rax
mov qword rax, [rsp+8]
mov qword [rel saved_reg_rip], rax
pop rax
mov qword [rel saved_reg_rsp], rsp
add qword [rel saved_reg_rsp], 24
mov qword [rel saved_reg_rbp], rbp
lea rsp, [rel safe_stack_top]
mov rbp, rsp
push qword 5
call deadly_exception
hlt
isr6:
cli
push rax
mov qword rax, [rsp+8]
mov qword [rel saved_reg_rip], rax
pop rax
mov qword [rel saved_reg_rsp], rsp
add qword [rel saved_reg_rsp], 24
mov qword [rel saved_reg_rbp], rbp
lea rsp, [rel safe_stack_top]
mov rbp, rsp
push qword 6
call deadly_exception
hlt
isr7:
cli
push rax
mov qword rax, [rsp+8]
mov qword [rel saved_reg_rip], rax
pop rax
mov qword [rel saved_reg_rsp], rsp
add qword [rel saved_reg_rsp], 24
mov qword [rel saved_reg_rbp], rbp
lea rsp, [rel safe_stack_top]
mov rbp, rsp
push qword 7
call deadly_exception
hlt
isr8:
cli
push rax
mov qword rax, [rsp+8]
mov qword [rel saved_reg_rip], rax
pop rax
mov qword [rel saved_reg_rsp], rsp
add qword [rel saved_reg_rsp], 32
mov qword [rel saved_reg_rbp], rbp
lea rsp, [rel safe_stack_top]
mov rbp, rsp
push qword 8
call deadly_exception
hlt
isr10:
cli
push rax
mov qword rax, [rsp+8]
mov qword [rel saved_reg_rip], rax
pop rax
mov qword [rel saved_reg_rsp], rsp
add qword [rel saved_reg_rsp], 32
mov qword [rel saved_reg_rbp], rbp
lea rsp, [rel safe_stack_top]
mov rbp, rsp
push qword 10
call deadly_exception
hlt
isr11:
cli
push rax
mov qword rax, [rsp+8]
mov qword [rel saved_reg_rip], rax
pop rax
mov qword [rel saved_reg_rsp], rsp
add qword [rel saved_reg_rsp], 32
mov qword [rel saved_reg_rbp], rbp
lea rsp, [rel safe_stack_top]
mov rbp, rsp
push qword 11
call deadly_exception
hlt
isr12:
cli
push rax
mov qword rax, [rsp+8]
mov qword [rel saved_reg_rip], rax
pop rax
mov qword [rel saved_reg_rsp], rsp
add qword [rel saved_reg_rsp], 32
mov qword [rel saved_reg_rbp], rbp
lea rsp, [rel safe_stack_top]
mov rbp, rsp
push qword 12
call deadly_exception
hlt
isr13:
cli
push rax
mov qword rax, [rsp+8]
mov qword [rel saved_reg_rip], rax
pop rax
mov qword [rel saved_reg_rsp], rsp
add qword [rel saved_reg_rsp], 32
mov qword [rel saved_reg_rbp], rbp
lea rsp, [rel safe_stack_top]
mov rbp, rsp
push qword 13
call deadly_exception
hlt
isr14:
cli
push rax
mov qword rax, [rsp+8]
mov qword [rel saved_reg_rip], rax
pop rax
mov qword [rel saved_reg_rsp], rsp
add qword [rel saved_reg_rsp], 32
mov qword [rel saved_reg_rbp], rbp
lea rsp, [rel safe_stack_top]
mov rbp, rsp
push qword 14
call deadly_exception
hlt
isr16:
cli
push rax
mov qword rax, [rsp+8]
mov qword [rel saved_reg_rip], rax
pop rax
mov qword [rel saved_reg_rsp], rsp
add qword [rel saved_reg_rsp], 24
mov qword [rel saved_reg_rbp], rbp
lea rsp, [rel safe_stack_top]
mov rbp, rsp
push qword 16
call deadly_exception
hlt
isr17:
cli
push rax
mov qword rax, [rsp+8]
mov qword [rel saved_reg_rip], rax
pop rax
mov qword [rel saved_reg_rsp], rsp
add qword [rel saved_reg_rsp], 32
mov qword [rel saved_reg_rbp], rbp
lea rsp, [rel safe_stack_top]
mov rbp, rsp
push qword 17
call deadly_exception
hlt
isr18:
cli
push rax
mov qword rax, [rsp+8]
mov qword [rel saved_reg_rip], rax
pop rax
mov qword [rel saved_reg_rsp], rsp
add qword [rel saved_reg_rsp], 24
mov qword [rel saved_reg_rbp], rbp
lea rsp, [rel safe_stack_top]
mov rbp, rsp
push qword 18
call deadly_exception
hlt
isr19:
cli
push rax
mov qword rax, [rsp+8]
mov qword [rel saved_reg_rip], rax
pop rax
mov qword [rel saved_reg_rsp], rsp
add qword [rel saved_reg_rsp], 24
mov qword [rel saved_reg_rbp], rbp
lea rsp, [rel safe_stack_top]
mov rbp, rsp
push qword 19
call deadly_exception
hlt
isr20:
cli
push rax
mov qword rax, [rsp+8]
mov qword [rel saved_reg_rip], rax
pop rax
mov qword [rel saved_reg_rsp], rsp
add qword [rel saved_reg_rsp], 24
mov qword [rel saved_reg_rbp], rbp
lea rsp, [rel safe_stack_top]
mov rbp, rsp
push qword 20
call deadly_exception
hlt
