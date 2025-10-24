global time_ms
global tick_cnt
global tick_per_ms
global timer_reset
global set_time_ms
global get_time_ms
global timer_get_tpms
global timer_set_tpms
section .data
time_ms dq 0
tick_cnt dq 0
tick_per_ms dq 0
section .text
timer_reset:
mov qword [rel time_ms], 0
mov qword [rel tick_cnt], 0
xor rax, rax
ret
set_time_ms:
cli
mov qword [rel time_ms], rcx
sti
xor rax, rax
ret
get_time_ms:
cli
mov qword rax, [rel time_ms]
sti
xor rax, rax
ret
timer_get_tpms:
mov qword rax, [tick_per_ms]
ret
timer_set_tpms:
mov qword [tick_per_ms], rcx
xor rax, rax
ret
