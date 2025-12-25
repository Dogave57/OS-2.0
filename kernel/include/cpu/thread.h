#ifndef _THREAD
#define _THREAD
#define THREAD_STATUS_INVALID ((uint64_t)0)
#define THREAD_STATUS_HALTED ((uint64_t)1)
#define THREAD_STATUS_RUNNING ((uint64_t)2)
#define THREAD_DEFAULT_STACK_SIZE (MEM_KB*32)
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
	struct thread_t* pFlink; // 152
	struct thread_t* pBlink; // 160
}__attribute__((packed));
int threads_init(void);
int thread_register(struct thread_t* pThread, uint64_t* pTid);
int thread_unregister(uint64_t tid);
int thread_create(uint64_t rip, uint64_t stackSize, uint64_t* pTid, uint64_t argument);
int thread_destroy(uint64_t tid);
int thread_get_status(uint64_t tid, uint64_t* pStatus);
#endif
