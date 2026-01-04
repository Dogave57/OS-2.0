#ifndef _THREAD
#define _THREAD
#include "kernel_include.h"
#define THREAD_STATUS_INVALID ((uint64_t)0)
#define THREAD_STATUS_HALTED ((uint64_t)1)
#define THREAD_STATUS_RUNNING ((uint64_t)2)
#define THREAD_DEFAULT_STACK_COMMIT (MEM_KB*8)
#define THREAD_DEFAULT_STACK_RESERVE (MEM_MB)
#define THREAD_DEFAULT_STACK_GUARD_SIZE (MEM_MB)
#define THREAD_PRIORITY_LOW 0x0
#define THREAD_PRIORITY_NORMAL 0x1
#define THREAD_PRIORITY_HIGH 0x2 
extern uint64_t ctx_switch_time;
struct thread_context_t{
	uint64_t rax;	// 0
	uint64_t rbx;	// 8
	uint64_t rcx;	// 16
	uint64_t rdx;	// 24
	uint64_t rsi;	// 32
	uint64_t rdi;	// 40
	uint64_t r8;	// 48
	uint64_t r9;	// 56
	uint64_t r10;	// 64
	uint64_t r11;	// 72
	uint64_t r12;	// 80
	uint64_t r13;	// 88
	uint64_t r14;	// 96
	uint64_t r15;	// 104
	uint64_t rsp;	// 112
	uint64_t rbp;	// 120
	uint64_t rip;	// 128
	uint64_t rflags; // 136
}__attribute__((packed));
struct thread_t{
	struct thread_context_t context; // 0
	uint64_t status; // 144
	uint64_t priority; // 152
	uint64_t tid; // 160
	uint64_t start_rip; // 168
	uint64_t pStack; // 176
	uint64_t stackGuardSize; // 184
	uint64_t stackReserve; // 192
	uint64_t stackCommit; // 200
	struct thread_t* pFlink; // 208
	struct thread_t* pBlink; // 216
}__attribute__((packed));
int threads_init(void);
int thread_link(struct thread_t* pThread, struct thread_t* pLink);
int thread_unlink(struct thread_t* pThread);
int thread_register(struct thread_t* pThread, uint64_t* pTid);
int thread_unregister(uint64_t tid);
KAPI int thread_create(uint64_t rip, uint64_t stackCommit, uint64_t stackReserve, uint64_t* pTid, uint64_t argument);
KAPI int thread_destroy(uint64_t tid);
KAPI int thread_get_status(uint64_t tid, uint64_t* pStatus);
KAPI int thread_set_status(uint64_t tid, uint64_t status);
KAPI int thread_get_priority(uint64_t tid, uint64_t* pPriority);
KAPI int thread_set_priority(uint64_t tid, uint64_t priority);
KAPI int thread_yield(void);
KAPI int get_current_thread(uint64_t* pTid);
KAPI int thread_exists(uint64_t tid);
__attribute__((ms_abi)) uint64_t get_rflags(void);
__attribute__((ms_abi)) int set_rflags(uint64_t rflags);
__attribute__((ms_abi)) int thread_destroy_safe(uint64_t tid);
#endif
