section .text
global get_rflags
global set_rflags
global thread_destroy_safe
extern pSafeStackTop
extern threadDestroySafe
extern thread_destroy
extern thread_yield
extern print
extern panic
extern thread_destroy
extern get_current_thread
extern scheduler_halt
extern scheduler_resume
extern pSafeStackTop
bad_safe_stack_msg db "bad safe stack", 10, 0
thread_destroy_fail_msg db "failed to destroy thread", 10, 0
thread_exit_safe_get_tid_fail_msg db "failed to get current thread TID when sfaely exiting", 10, 0
thread_exit_safe_destroy_fail_msg db "failed to destroy current thread when safely exiting", 10, 0
get_rflags:
pushfq
mov qword rax, [rsp]
add qword rsp, 8
ret
set_rflags:
push rcx
popfq
xor rax, rax
ret
thread_destroy_safe:
mov qword rax, [rel pSafeStackTop]
cmp rax, 0
je thread_destroy_safe_bad_stack
mov qword rsp, rax
mov qword [rel threadDestroySafe], 1
sub qword rsp, 32
xor rax, rax
call thread_destroy
add qword rsp, 32
cmp rax, 0
jne thread_destroy_fail
loop0:
sub qword rsp, 32
call thread_yield
add qword rsp, 32
jmp loop0
hlt
ret
thread_destroy_fail:
mov qword rcx, thread_destroy_fail_msg
sub qword rsp, 32
call print
add qword rsp, 32
loop2:
sub qword rsp, 32
call thread_yield
add qword rsp, 32
jmp loop2
ret
thread_destroy_safe_bad_stack:
mov qword rcx, bad_safe_stack_msg
sub qword rsp, 32
call print
add qword rsp, 32
loop1:
sub qword rsp, 32
call thread_yield
add qword rsp, 32
jmp loop1
thread_exit_safe:
cli
mov qword rsp, [rel pSafeStackTop]

hlt
sub qword rsp, 32
add qword rsp, 32
sub qword rsp, 32
call scheduler_halt
add qword rsp, 32
sub qword rsp, 8
mov qword rcx, [rsp]
sub qword rsp, 40
call get_current_thread
add qword rsp, 40
cmp rax, 0
jne thread_exit_safe_get_tid_fail
mov qword rcx, [rsp]
add qword rsp, 8
sub qword rsp, 32
call thread_destroy
add qword rsp, 32
cmp rax, 0
jne thread_exit_safe_destroy_fail
sti
sub qword rsp, 32
call scheduler_resume
add qword rsp, 32
thread_exit_safe_loop:
jmp thread_exit_safe_loop
thread_exit_safe_get_tid_fail:
sub qword rsp, 32
mov qword rcx, thread_exit_safe_get_tid_fail_msg
call panic
add qword rsp, 32
thread_exit_safe_destroy_fail:
sub qword rsp, 32
mov qword rcx, thread_exit_safe_destroy_fail_msg
call panic
add qword rsp, 32
