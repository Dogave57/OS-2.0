global time_ms
global tick_cnt
global tick_per_ms
global timer_reset
global set_time_ms
global get_time_ms
global timer_get_tpms
global timer_set_tpms
global sleep
section .data
time_ms dq 0
tick_cnt dq 0
tick_per_ms dq 0
section .text
timer_reset:
cli
mov qword [rel time_ms], 0
mov qword [rel tick_cnt], 0
sti
xor rax, rax
ret
set_time_ms:
cli
mov qword [rel time_ms], rdi
sti
xor rax, rax
ret
get_time_ms:
cli
mov qword rax, [rel time_ms]
sti
ret
timer_get_tpms:
cli
mov qword rax, [rel tick_per_ms]
sti
ret
timer_set_tpms:
cli
mov qword [rel tick_per_ms], rdi
sti
xor rax, rax
ret
sleep:
call get_time_ms
mov rdx, rax
sleep_loop:
call get_time_ms
sub rax, rdx
cmp rax, rdi
jb sleep_loop
xor rax, rax
ret
