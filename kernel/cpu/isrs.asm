%macro pushaq 0
push rbp
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
pushfq
%endmacro
%macro popaq 0
popfq
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
pop rbp
%endmacro
%macro ISR_STUB 1
global isr_stub_%+%1
isr_stub_%+%1:
cli
push rbx
push rcx
mov qword rbx, isr_stub_target_list
add qword rbx, (%1*8)
mov qword rbx, [rbx]
mov qword rcx, %1
sub qword rsp, 32
call rbx
add qword rsp, 32
pop rcx
pop rbx
iretq
%endmacro
section .data
target_isr_msg db "target ISR: %p", 10, 0
isr_stub_target_list:
%rep 256
dq 0
%endrep
isr_stub_target_list_end:
isr_stub_list:
%assign isr_stub_list_index 0
%rep 256
dq isr_stub_%+isr_stub_list_index
%assign isr_stub_list_index isr_stub_list_index+1
%endrep
isr_stub_list_end:
global isr_stub_target_list
global isr_stub_list
exception_names:
de_name db "divide by zero", 10, 0
db_name db "debug", 10, 0
nmi_name db "non-maskable interrupt", 10, 0
bp_name db "breakpoint", 10, 0
of_name db "overflow", 10, 0
ud_name db "invalid opcode", 10, 0
br_name db "bound range exceeded", 10, 0
nm_name db "device not available", 10, 0
df_name db "double fault", 10, 0
csor_name db "coprocessor segment overrun", 10, 0
ts_name db "invalid TSS", 10, 0
np_name db "segment not present", 10, 0
ss_name db "stack segmentation fault", 10, 0
gp_name db "general protection fault", 10, 0
pf_name db "page fault", 10, 0
mf_name db "x87 floating point exception", 10, 0
ac_name db "alignment check", 10, 0
mc_name db "machine check", 10, 0
xm_name db "simd floating point", 10, 0
ve_name db "virtualization", 10, 0
cp_name db "control protection", 10, 0
hv_name db "hypervisor injection", 10, 0
vc_name db "vmm communication", 10, 0
sx_name db "security", 10, 0
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
exception_args:
dq 0 ; code - 0
dq 0 ; rip - 8
dq 0 ; rax - 16
dq 0 ; rbx - 24
dq 0 ; rcx - 32
dq 0 ; rdx - 40
dq 0 ; rdi - 48
dq 0 ; rsi - 56
dq 0 ; r8 - 64
dq 0 ; r9 - 72 
dq 0 ; r10 - 80
dq 0 ; r11 - 88 
dq 0 ; r12 - 96
dq 0 ; r13 - 104 
dq 0 ; r14 - 112
dq 0 ; r15 - 120
dq 0 ; rbp - 128 
dq 0 ; rsp - 136
dq 0 ; cr0 - 144 
dq 0 ; cr2 - 152
dq 0 ; cr3 - 160
dq 0 ; cs - 168
dq 0 ; ds - 176
dq 0 ; gs - 184
dq 0 ; ss - 192
dq 0 ; error code - 200
dq 0 ; contains error code - 208
dq 0 ; rflags - 216
ctx_switch_args:
dq 0 ; rax - 0
dq 0 ; rbx - 8
dq 0 ; rcx - 16
dq 0 ; rdx - 24
dq 0 ; rdi - 32
dq 0 ; rsi - 40
dq 0 ; r8 - 48
dq 0 ; r9 - 56
dq 0 ; r10 - 64
dq 0 ; r11 - 72
dq 0 ; r12 - 80
dq 0 ; r13 - 88
dq 0 ; r14 - 96
dq 0 ; r15 - 104
dq 0 ; rbp - 112
dq 0 ; rsp - 120
dq 0 ; rip - 128
saved_registers:
saved_reg_rip:
dq 0
saved_reg_rsp:
dq 0
saved_reg_rbp:
dq 0
section .data
exceptionmsg:
db "exception: %d", 10, 0
regmsg:
db "rax: %p rbx: %p rcx: %p rdx: %p", 10, "rdi: %p rsi: %p r8: %p r9: %p", 10, "r10: %p r11: %p r12: %p r13: %p", 10, "r14: %p r15: %p rbp: %p rsp: %p", 10, "cr0: %p cr2: %p cr3: %p", 10, "cs: %p ds: %p gs: %p ss: %p", 10, "rip: %p", 10, "rflags: 0x%x", 10, 0 
pf_dump_msg:
db "page fault virtual address: %p (cr2)", 10, 0
pf_np_msg:
db "unmapped page violation", 10, 0
pf_pv_msg:
db "protection violation", 10, 0
pf_rf_msg:
db "read violation", 10, 0
pf_wf_msg:
db "write violation", 10, 0
pf_if_msg:
db "instruction fetch violation", 10, 0
pf_kv_msg:
db "kernel page violation", 10, 0
pf_uv_msg:
db "user page violation", 10, 0
pf_rs_msg:
db "reserved bit violation", 10, 0
error_code_dump_msg:
db "error code: 0x%x", 10, 0
thread_dump_msg:
db "thread status dump", 10, 0
tid_dump_msg:
db "tid: %d", 10, 0
thread_rip_dump_msg:
db "starting RIP: %p", 10, 0
thread_rsp_dump_msg:
db "starting RSP: %p", 10, 0
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
global ctx_switch_time
global xhci_interrupter_isr
global nvme_completion_queue_isr
extern print
extern lprint
extern printf
extern lprintf
extern clear
extern set_text_color
extern lapic_send_eoi
extern thermal_state
extern putchar
extern ps2_keyboard_handler
extern crypto_entropy
extern entropy_shuffle
extern pFirstThread
extern pLastThread
extern pCurrentThread
extern lapic_set_tick_us
extern virtualMapPage
extern vmm_getPageTableEntry
extern physicalAllocPage
extern virtualGetPageFlags
extern lapic_tick_count
extern xhci_interrupter
extern nvme_completion_queue_interrupt
extern schedulerHalt
exception_fg:
db 255, 255, 255, 0
exception_bg:
db 0, 0, 0, 0
deadly_exception:
sub rsp, 32
mov qword rcx, exception_fg
mov qword rdx, exception_bg
call set_text_color
call clear
add rsp, 32
mov qword rax, [rel exception_args]
push rax
mov qword rcx, exceptionmsg
mov qword rdx, rax
sub rsp, 32
call printf
add rsp, 32
pop rax
mov qword rbx, exception_name_map
imul rax, 8
add qword rbx, rax
mov qword rbx, [rbx]
cmp rbx, 0
je b
sub rsp, 32
mov qword rcx, rbx
call print
add rsp, 32
mov qword rcx, regmsg
mov qword rdx, [rel exception_args+16] ; rax
mov qword r8, [rel exception_args+24] ; rbx
mov qword r9, [rel exception_args+32] ; rcx
push qword [rel exception_args+216] ; rflags
push qword [rel exception_args+8] ; rip
push qword [rel exception_args+192] ; ss
push qword [rel exception_args+184] ; gs
push qword [rel exception_args+176] ; ds
push qword [rel exception_args+168] ; cs
push qword [rel exception_args+160] ; cr3
push qword [rel exception_args+152] ; cr2
push qword [rel exception_args+144] ; cr0
push qword [rel exception_args+136] ; rsp
push qword [rel exception_args+128] ; rbp
push qword [rel exception_args+120] ; r15
push qword [rel exception_args+112] ; r14
push qword [rel exception_args+104] ; r13
push qword [rel exception_args+96] ; r12
push qword [rel exception_args+88] ; r11
push qword [rel exception_args+80] ; r10
push qword [rel exception_args+72] ; r9
push qword [rel exception_args+64] ; r8
push qword [rel exception_args+56] ; rsi
push qword [rel exception_args+48] ; rdi
push qword [rel exception_args+40] ; rdx
sub rsp, 32
call printf
add rsp, 32
mov qword rax, [rel exception_args]
cmp rax, 14
jne pf_special_handler_end
pf_special_handler:
dump_pf_addr:
mov qword rcx, pf_dump_msg
mov qword rdx, cr2
sub rsp, 32
call printf
add rsp, 32
dump_pf_addr_end:
dump_pf_error_code:
mov qword rax, [rel exception_args+200]
test rax, (1<<0)
jnz dump_pf_pv
dump_pf_np:
mov qword rcx, pf_np_msg
push rax
sub rsp, 32
call print
add rsp, 32
pop rax
jmp dump_pf_pv_end
dump_pf_np_end:
dump_pf_pv:
mov qword rcx, pf_pv_msg
push rax
sub rsp, 32
call print
add rsp, 32
pop rax
dump_pf_pv_end:
test rax, (1<<1)
jnz dump_pf_wf
dump_pf_rf:
push rax
mov qword rcx, pf_rf_msg
sub rsp, 32
call print
add rsp, 32
pop rax
jmp dump_pf_wf_end
dump_pf_rf_end:
dump_pf_wf:
push rax
mov qword rcx, pf_wf_msg
sub rsp, 32
call print
add rsp, 32
pop rax
dump_pf_wf_end:
test rax, (1<<4)
jz dump_pf_if_end
dump_pf_if:
push rax
mov qword rcx, pf_if_msg
sub rsp, 32
call print
add rsp, 32
pop rax
dump_pf_if_end:
test rax, (1<<2)
jnz dump_pf_uv
dump_pf_kv:
push rax
mov qword rcx, pf_kv_msg
sub rsp, 32
call printf
add rsp, 32
pop rax
jmp dump_pf_uv_end
dump_pf_kv_end:
dump_pf_uv:
push rax
mov qword rcx, pf_uv_msg
sub rsp, 32
call print
add rsp, 32
pop rax
dump_pf_uv_end:
test rax, (1<<3)
jnz dump_pf_rs
jmp dump_pf_rs_end
dump_pf_rs:
push rax
mov qword rcx, pf_rs_msg
sub rsp, 32
call print
add rsp, 32
pop rax
dump_pf_rs_end:
dump_pf_error_code_end:
pf_special_handler_end:
mov qword rax, [rel exception_args+208]
cmp rax, 0
je dump_error_code_end
dump_error_code:
mov qword rcx, error_code_dump_msg
mov qword rdx, [rel exception_args+200]
sub rsp, 32
call printf
add rsp, 32
dump_error_code_end:
dump_thread_info:
mov qword rax, [rel pCurrentThread]
cmp rax, 0
je dump_thread_info_end
mov qword rcx, tid_dump_msg
mov qword rdx, [rax+160]
push rax
sub qword rsp, 32
call printf
add qword rsp, 32
pop rax
mov qword rcx, thread_rip_dump_msg
mov qword rdx, [rax+168]
push rax
sub qword rsp, 32
call printf
add qword rsp, 32
pop rax
mov qword rcx, thread_rsp_dump_msg
mov qword rdx, [rax+176]
push rax
sub qword rsp, 32
call printf
add qword rsp, 32
pop rax
dump_thread_info_end:
b:
jmp b
hlt
ret
default_isr:
cli
sub qword rsp, 32
call lapic_send_eoi
add qword rsp, 32
iretq
pic_timer_isr:
cli
pushaq
mov al, 20h
mov dx, 20h
out dx, al
popaq
iretq
msg db "swapped registers out", 10, 0
save_msg db "saving registers", 10, 0
switch_msg db "switching tasks", 10, 0
rip_msg db "RIP: %p", 10, 0
rsp_msg db "RSP: %p", 10, 0
ctx_no_rsp_msg db "invalid RSP", 10, 0
ctx_no_rip_msg db "invalid RIP", 10, 0
current_thread_msg db "current thread ID: %d", 10, 0
rflags_msg db "RFLAGS: %p", 10, 0
ctx_switch:
mov qword rax, [rel schedulerHalt]
cmp rax, 0
jne ctx_switch_end
mov qword rax, [rel pFirstThread]
cmp rax, 0
je ctx_switch_end
popaq
mov qword [rel ctx_switch_args], rax
mov qword rax, [rel pCurrentThread]
cmp rax, 0
jne ctx_switch_next_thread
ctx_switch_first_thread:
mov qword rax, [rel pFirstThread]
jmp ctx_switch_next_thread_end
ctx_switch_next_thread:
mov qword [rax+8], rbx
mov qword [rax+16], rcx
mov qword [rax+24], rdx
mov qword [rax+32], rsi
mov qword [rax+40], rdi
mov qword [rax+48], r8
mov qword [rax+56], r9
mov qword [rax+64], r10
mov qword [rax+72], r11
mov qword [rax+80], r12
mov qword [rax+88], r13
mov qword [rax+96], r14
mov qword [rax+104], r15
mov qword rbx, [rsp+24]
mov qword [rax+112], rbx
mov qword [rax+120], rbp
mov qword rbx, [rsp]
mov qword [rax+128], rbx
mov qword rbx, [rel ctx_switch_args]
mov qword [rax], rbx
mov qword rbx, [rsp+16]
mov qword [rax+136], rbx
mov qword rax, [rax+208]
cmp rax, 0
je ctx_switch_first_thread
ctx_switch_next_thread_end:
mov qword [rel pCurrentThread], rax
priority_check:
mov qword rbx, [rax+152]
cmp rbx, 0
je priority_case_low
cmp rbx, 2
je priority_case_high
priority_case_normal:
mov qword rcx, 1000
jmp priority_check_end
priority_case_high:
mov qword rcx, 5000
jmp priority_check_end
priority_case_low:
mov qword rcx, 500
priority_check_end:
push rax
sub qword rsp, 32
call lapic_set_tick_us
add qword rsp, 32
pop rax
mov qword rbx, [rax+112]
mov qword [rsp+24], rbx
mov qword rbx, [rax+128]
mov qword [rsp], rbx
switch_rflags:
mov qword rbx, [rax+136]
mov qword [rsp+16], rbx
switch_rflags_end:
mov qword rbx, [rax+8]
mov qword rcx, [rax+16]
mov qword rdx, [rax+24]
mov qword rsi, [rax+32]
mov qword rdi, [rax+40]
mov qword r8, [rax+48]
mov qword r9, [rax+56]
mov qword r10, [rax+64]
mov qword r11, [rax+72]
mov qword r12, [rax+80]
mov qword r13, [rax+88]
mov qword r14, [rax+96]
mov qword r15, [rax+104]
mov qword rbp, [rax+120]
mov qword rax, [rax]
pushaq
ctx_switch_end:
jmp timer_isr_end
ctx_switch_time dq 10
timer_isr:
cli
pushaq
add qword [rel lapic_tick_count], 10
jmp ctx_switch
timer_isr_end:
mov qword rbp, rsp
and rsp, -16
sub rsp, 32
call entropy_shuffle
add rsp, 32
sub rsp, 32
call lapic_send_eoi
add rsp, 32
mov qword rsp, rbp
popaq
iretq
thermal_isr:
cli
mov qword rbp, rsp
and rsp, -16
sub rsp, 32
call lapic_send_eoi
add rsp, 32
mov qword rsp, rbp
sti
iretq
ps2_kbd_isr:
cli
pushaq
mov qword rbp, rsp
and rsp, -16
sub rsp, 32
call ps2_keyboard_handler
call entropy_shuffle
call lapic_send_eoi
add rsp, 32
mov qword rsp, rbp
popaq
iretq
xhci_interrupter_isr:
cli
pushaq
mov qword rbp, rsp
and rsp, -16
sub qword rsp, 32
call xhci_interrupter
add qword rsp, 32
sub qword rsp, 32
call entropy_shuffle
add qword rsp, 32
sub qword rsp, 32
call lapic_send_eoi
add qword rsp, 32
mov qword rsp, rbp
popaq
iretq
nvme_completion_queue_isr:
cli
pushaq
mov qword rbp, rsp
and rsp, -16
sub qword rsp, 32
call nvme_completion_queue_interrupt
add qword rsp, 32
sub qword rsp, 32
call entropy_shuffle
add qword rsp, 32
sub qword rsp, 32
call lapic_send_eoi
add qword rsp, 32
mov qword rsp, rbp
popaq	
ret
isr0_msg db "divide by zero ISR triggered", 10, 0
exception_handler_entry:
mov qword [rel exception_args+16], rax
mov qword [rel exception_args+24], rbx
mov qword [rel exception_args+32], rcx
mov qword [rel exception_args+40], rdx
mov qword [rel exception_args+48], rdi
mov qword [rel exception_args+56], rsi
mov qword [rel exception_args+64], r8
mov qword [rel exception_args+72], r9
mov qword [rel exception_args+80], r10
mov qword [rel exception_args+88], r11
mov qword [rel exception_args+96], r12
mov qword [rel exception_args+104], r13
mov qword [rel exception_args+112], r14
mov qword [rel exception_args+120], r15
mov qword [rel exception_args+128], rbp
mov qword rax, [rsp+32]
mov qword [rel exception_args+136], rax
mov qword rax, cr0
mov qword [rel exception_args+144], rax
mov qword rax, cr2
mov qword [rel exception_args+152], rax
mov qword rax, cr3
mov qword [rel exception_args+160], rax
mov qword rax, [rsp+16]
mov qword [rel exception_args+168], rax
mov qword rax, ds
mov qword [rel exception_args+176], rax
mov qword rax, gs
mov qword [rel exception_args+184], rax
mov qword rax, ss
mov qword [rel exception_args+192], rax
mov qword rax, [rsp+8]
mov qword [rel exception_args+8], rax
mov qword rax, [rsp]
mov qword [rel exception_args], rax
mov qword [rel exception_args+208], 0
mov qword rax, [rsp+24]
mov qword [rel exception_args+216], rax
jmp deadly_exception
exception_handler_entry_error_code:
mov qword [rel exception_args+16], rax
mov qword [rel exception_args+24], rbx
mov qword [rel exception_args+32], rcx
mov qword [rel exception_args+40], rdx
mov qword [rel exception_args+48], rdi
mov qword [rel exception_args+56], rsi
mov qword [rel exception_args+64], r8
mov qword [rel exception_args+72], r9
mov qword [rel exception_args+80], r10
mov qword [rel exception_args+88], r11
mov qword [rel exception_args+96], r12
mov qword [rel exception_args+104], r13
mov qword [rel exception_args+112], r14
mov qword [rel exception_args+120], r15
mov qword [rel exception_args+128], rbp
mov qword rax, [rsp+40]
mov qword [rel exception_args+136], rax
mov qword rax, cr0
mov qword [rel exception_args+144], rax
mov qword rax, cr2
mov qword [rel exception_args+152], rax
mov qword rax, cr3
mov qword [rel exception_args+160], rax
mov qword rax, [rsp+24]
mov qword [rel exception_args+168], rax
mov qword rax, ds
mov qword [rel exception_args+176], rax
mov qword rax, gs
mov qword [rel exception_args+184], rax
mov qword rax, ss
mov qword [rel exception_args+192], rax
mov qword rax, [rsp+16]
mov qword [rel exception_args+8], rax
mov qword rax, [rsp]
mov qword [rel exception_args], rax
mov qword rax, [rsp+8]
mov qword [rel exception_args+200], rax
mov qword [rel exception_args+208], 1
mov qword rax, [rsp+32]
mov qword [rel exception_args+216], rax
jmp deadly_exception
exception_isr_error_code:
isr0:
cli
sub rsp, 8
mov qword [rsp], 0
jmp exception_handler_entry
hlt
isr1:
cli
sub rsp, 8
mov qword [rsp], 1
jmp exception_handler_entry
hlt
isr2:
cli
sub rsp, 8
mov qword [rsp], 2
jmp exception_handler_entry
hlt
isr3:
cli
sub rsp, 8
mov qword [rsp], 3
jmp exception_handler_entry
hlt
isr4:
cli
sub rsp, 8
mov qword [rsp], 4
jmp exception_handler_entry
hlt
isr5:
cli
sub rsp, 8
mov qword [rsp], 5
jmp exception_handler_entry
hlt
isr6:
cli
sub rsp, 8
mov qword [rsp], 6
jmp exception_handler_entry
hlt
isr7:
cli
sub rsp, 8
mov qword [rsp], 7
jmp exception_handler_entry
hlt
isr8:
cli
sub rsp, 8
mov qword [rsp], 8
jmp exception_handler_entry_error_code
hlt
isr10:
cli
sub rsp, 8
mov qword [rsp], 10
jmp exception_handler_entry_error_code
hlt
isr11:
cli
sub rsp, 8
mov qword [rsp], 11
jmp exception_handler_entry_error_code
hlt
isr12:
cli
sub rsp, 8
mov qword [rsp], 12
jmp exception_handler_entry_error_code
hlt
isr13:
cli
sub rsp, 8
mov qword [rsp], 13
jmp exception_handler_entry_error_code
hlt
pa_msg db "physical address: %p", 10, 0
lazy_msg db "lazy page", 10, 0
lazy_physical_alloc_msg db "physical page: %p", 10, 0
lazy_physical_alloc_fail_msg db "failed to allocate physical page", 10, 0
lazy_virtual_map_fail_msg db "failed to remap lazy page", 10, 0
isr14:
cli
pte_check:
pushaq
mov qword rcx, cr2
sub qword rsp, 8
mov qword rdx, rsp
sub qword rsp, 32
call vmm_getPageTableEntry
add qword rsp, 32
mov qword rbx, [rsp]
mov qword rcx, [rbx]
add qword rsp, 8
mov qword rdx, (1<<9)
and rcx, rdx
cmp rcx, 0
je pte_case_lazy_end
pte_case_lazy:
mov qword rcx, [rbx]
sub qword rsp, 8
mov qword rdx, rsp
push rbx
push rcx
mov qword rcx, rdx
mov qword rdx, 2
sub qword rsp, 32
call physicalAllocPage
add qword rsp, 32
pop rcx
pop rbx
mov qword rdx, [rsp]
add qword rsp, 8
cmp rax, 0
jne pte_physical_alloc_fail
push rbx
push rcx
push rdx
sub qword rsp, 8
mov qword rcx, cr2
mov qword rdx, rsp
sub qword rsp, 32
call virtualGetPageFlags
add qword rsp, 32
mov qword r8, [rsp]
add qword rsp, 8
pop rdx
pop rcx
pop rbx
mov qword rax, 2
push rax
mov qword rax, 0
push rax
mov qword rcx, rdx
mov qword rdx, cr2
mov qword r9, 1
sub qword rsp, 32
call virtualMapPage
add qword rsp, 32
add qword rsp, 16
cmp rax, 0
jne pte_virtual_map_fail
popaq
add qword rsp, 8
iretq
pte_physical_alloc_fail:
mov qword rcx, lazy_physical_alloc_fail_msg
sub qword rsp, 32
call print
add qword rsp, 32
hlt
pte_virtual_map_fail:
mov qword rcx, lazy_virtual_map_fail_msg
sub qword rsp, 32
call print
add qword rsp, 32
hlt
pte_case_lazy_end:
pte_check_end:
popaq
sub rsp, 8
mov qword [rsp], 14
jmp exception_handler_entry_error_code
hlt
isr16:
cli
sub rsp, 8
mov qword [rsp], 16
jmp exception_handler_entry
hlt
isr17:
cli
sub rsp, 8
mov qword [rsp], 17
jmp exception_handler_entry_error_code
hlt
isr18:
cli
sub rsp, 8
mov qword [rsp], 18
jmp exception_handler_entry
hlt
isr19:
cli
sub rsp, 8
mov qword [rsp], 19
jmp exception_handler_entry
hlt
isr20:
cli
sub rsp, 8
mov qword [rsp], 20
jmp exception_handler_entry
hlt
%assign isr_stub_index 0
%rep 256
ISR_STUB isr_stub_index
%assign isr_stub_index isr_stub_index+1
%endrep
