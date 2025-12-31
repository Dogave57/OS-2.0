global time_ms
global tick_cnt
global tick_per_ms
global timer_reset
global set_time_ms
global get_time_ms
global timer_get_tpms
global timer_set_tpms
global sleep
global lapic_tick_count
extern hpet_get_ms
extern hpet_set_ms
section .data
lapic_tick_count dq 0
tick_per_ms dq 0
section .text
timer_reset:
xor rcx, rcx
;call hpet_set_ms
xor rax, rax
ret
set_time_ms:
;call hpet_set_ms
ret
get_time_ms:
call hpet_get_ms
ret
timer_get_tpms:
mov qword rax, [rel tick_per_ms]
ret
timer_set_tpms:
mov qword [rel tick_per_ms], rcx
xor rax, rax
ret
sleep:
push rcx
call get_time_ms
pop rcx
mov qword rbx, rax
sleep_loop:
push rcx
call get_time_ms
pop rcx
sub rax, rbx
cmp rax, rcx
jb sleep_loop
sleep_loop_end:
xor rax, rax
ret

