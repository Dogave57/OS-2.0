global ticks_per_us
global timer_reset
global set_time_us
global get_time_us
global timer_get_tpus
global timer_set_tpus
global sleep
global sleep_us
global lapic_tick_count
extern hpet_get_us
extern hpet_set_us
extern thread_yield
section .data
ticks_per_us dq 0
lapic_tick_count dq 0
section .text
timer_reset:
xor rcx, rcx
sub qword rsp, 40
;call hpet_set_us
add qword rsp, 40 
xor rax, rax
ret
set_time_us:
sub qword rsp, 40
;call hpet_set_us
add qword rsp, 40
ret
get_time_us:
sub qword rsp, 40
call hpet_get_us
add qword rsp, 40
ret
get_time_ms:
sub qword rsp, 40
call get_time_us
add qword rsp, 40
mov qword rbx, 1000
xor rdx, rdx
div rbx
ret
set_time_ms:
imul rcx, 1000
sub qword rsp, 40
call get_time_us
add qword rsp, 40
xor rax, rax
ret
timer_get_tpus:
mov qword rax, [rel ticks_per_us]
ret
timer_set_tpus:
mov qword [rel ticks_per_us], rcx
xor rax, rax
ret
sleep:
push rcx
sub qword rsp, 32
call get_time_ms
add qword rsp, 32
pop rcx
mov qword rbx, rax
sleep_loop:
sub qword rsp, 8
push rcx
push rbx
sub qword rsp, 32
call get_time_ms
add qword rsp, 32
pop rbx
pop rcx
add qword rsp, 8
sub rax, rbx
cmp rax, rcx
jae sleep_loop_end
sub qword rsp, 8
push rbx
push rcx
sub qword rsp, 32
call thread_yield
add qword rsp, 32
pop rcx
pop rbx
add qword rsp, 8
jmp sleep_loop
sleep_loop_end:
xor rax, rax
ret
sleep_us:
sub qword rsp, 8
push rcx
sub qword rsp, 32
call get_time_us
add qword rsp, 32
pop rcx
add qword rsp, 8
mov qword rbx, rax
sleep_us_loop:
push rbx
push rcx
sub qword rsp, 32
call get_time_us
add qword rsp, 32
pop rcx
pop rbx
sub qword rax, rbx
cmp rax, rcx
jae sleep_us_loop_end
push rbx
push rcx
sub qword rsp, 32
call thread_yield
add qword rsp, 32
pop rcx
pop rbx
jmp sleep_us_loop
sleep_us_loop_end:
;mov qword rsp, rbp
ret

