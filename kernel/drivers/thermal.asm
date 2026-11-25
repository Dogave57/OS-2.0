global get_thermal_state
section .data
thermal_state dq 0
section .text
get_thermal_state:
mov qword rax, [rel thermal_state]
ret
set_thermal_state:
mov qword [rel thermal_state], rcx
xor rax, rax
ret
