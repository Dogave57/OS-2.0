#include "mem/vmm.h"
#include "mem/heap.h"
#include "align.h"
#include "drivers/timer.h"
#include "stdlib/stdlib.h"
#include "subsystem/subsystem.h"
#include "cpu/interrupt.h"
#include "cpu/thread.h"
static struct subsystem_desc* pSubsystemDesc = (struct subsystem_desc*)0x0;
struct thread_t* pFirstThread = (struct thread_t*)0x0;
struct thread_t* pLastThread = (struct thread_t*)0x0;
struct thread_t* pCurrentThread = (struct thread_t*)0x0;
unsigned char threadDestroySafe = 0;
int threads_init(void){
	if (subsystem_init(&pSubsystemDesc, 65536)!=0)
		return -1;
	return 0;
}
int thread_link(struct thread_t* pThread, struct thread_t* pLink){
	if (!pThread)
		return -1;
	uint64_t old_rflags = get_rflags();
	__asm__ volatile("cli");
	if (!pLink)
		pLink = pLastThread;
	if (!pFirstThread)
		pFirstThread = pThread;
	if (pLink){
		pLink->pFlink = pThread;
		pThread->pBlink = pLink;
		if (pLink==pLastThread)
			pThread->pFlink = pFirstThread;
	}
	if (pLink==pLastThread)
		pLastThread = pThread;
	set_rflags(old_rflags);
	return 0;
}
int thread_unlink(struct thread_t* pThread){
	if (!pThread)
		return -1;
	uint64_t old_rflags = get_rflags();
	__asm__ volatile("cli");
	if (pFirstThread==pThread){
		pFirstThread = pThread->pFlink;
	}
	if (pLastThread==pThread){
		pLastThread = pThread->pBlink;
	}
	if (pThread->pFlink){
		pThread->pFlink->pBlink = pThread->pBlink;
	}
	if (pThread->pBlink){
		pThread->pBlink->pFlink = pThread->pFlink;
	}
	set_rflags(old_rflags);
	return 0;
}
int thread_register(struct thread_t* pThread, uint64_t* pTid){
	if (!pThread||!pTid)
		return -1;
	uint64_t old_rflags = get_rflags();
	__asm__ volatile("cli");
	uint64_t tid = 0;
	if (subsystem_alloc_entry(pSubsystemDesc, (unsigned char*)pThread, &tid)!=0){
		set_rflags(old_rflags);
		return -1;
	}
	if (thread_link(pThread, (struct thread_t*)0x0)!=0){
		subsystem_free_entry(pSubsystemDesc, tid);
		set_rflags(old_rflags);
		return -1;
	}
	*pTid = tid;
	set_rflags(old_rflags);
	return 0;
}
int thread_unregister(uint64_t tid){
	uint64_t old_rflags = get_rflags();
	__asm__ volatile("cli");
	struct thread_t* pThread = (struct thread_t*)0x0;
	if (subsystem_get_entry(pSubsystemDesc, tid, (uint64_t*)&pThread)!=0){
		set_rflags(old_rflags);
		return -1;
	}
	if (!pThread){
		set_rflags(old_rflags);
		return -1;
	}
	if (subsystem_free_entry(pSubsystemDesc, tid)!=0){
		set_rflags(old_rflags);
		return -1;
	}
	if (thread_unlink(pThread)!=0){
		set_rflags(old_rflags);
		return -1;
	}
	set_rflags(old_rflags);
	return 0;
}
KAPI int thread_create(uint64_t rip, uint64_t stackCommit, uint64_t stackReserve, uint64_t* pTid, uint64_t argument){
	if (!pTid||!rip)
		return -1;
	uint64_t old_rflags = get_rflags();
	__asm__ volatile("cli");
	if (!stackCommit)
		stackCommit = THREAD_DEFAULT_STACK_COMMIT;
	if (!stackReserve)
		stackReserve = THREAD_DEFAULT_STACK_RESERVE;
	uint64_t stackCommitPages = align_up(stackCommit, PAGE_SIZE)/PAGE_SIZE;
	unsigned char* pStack = (unsigned char*)0x0;
	unsigned char* pLastStackGuard = (unsigned char*)0x0;
	uint64_t stackGuardSize = MEM_MB;
	uint64_t stackSize = (stackGuardSize*2)+stackReserve;
	if (virtualAlloc((uint64_t*)&pStack, stackSize, PTE_RW|PTE_NX, MAP_FLAG_LAZY, PAGE_TYPE_NORMAL)!=0){
		set_rflags(old_rflags);
		return -1;
	}
	pLastStackGuard = pStack+stackGuardSize+stackReserve;
	if (virtualMapPages((uint64_t)0x0, (uint64_t)pStack, 0, align_up(stackGuardSize, PAGE_SIZE)/PAGE_SIZE, 1, 0, PAGE_TYPE_NORMAL)!=0){
		virtualFree((uint64_t)pStack, stackSize);
		set_rflags(old_rflags);
		return -1;
	}
	if (virtualMapPages((uint64_t)0x0, (uint64_t)pLastStackGuard, 0, align_up(stackGuardSize, PAGE_SIZE)/PAGE_SIZE, 1, 0, PAGE_TYPE_NORMAL)!=0){
		virtualFree((uint64_t)pStack, stackSize);
		set_rflags(old_rflags);
		return -1;
	}
	pStack+=stackGuardSize;
	for (uint64_t i = 0;i<stackCommitPages;i++){
		uint64_t pa = 0;
		uint64_t va = ((uint64_t)pStack)+stackReserve-(i*PAGE_SIZE);
		if (physicalAllocPage(&pa, PAGE_TYPE_NORMAL)!=0){
			virtualFree((uint64_t)pStack, stackSize);
			set_rflags(old_rflags);
			return -1;
		}
		if (virtualMapPage(pa, va, PTE_RW|PTE_NX, 1, 0, PAGE_TYPE_NORMAL)!=0){
			virtualFree((uint64_t)pStack, stackSize);
			physicalFreePage(pa);
			set_rflags(old_rflags);
			return -1;
		}
	}
	struct thread_t* pThread = (struct thread_t*)kmalloc(sizeof(struct thread_t));
	if (!pThread){
		virtualFree((uint64_t)pStack, stackSize);
		set_rflags(old_rflags);
		return -1;
	}
	memset((void*)pThread, 0, sizeof(struct thread_t));
	pThread->pStack = (uint64_t)(pStack-stackGuardSize);
	pThread->stackGuardSize = stackGuardSize;
	pThread->stackCommit = stackCommit;
	pThread->stackReserve = stackReserve;
	uint64_t rsp = ((uint64_t)pStack)+stackReserve-64;
	struct thread_context_t* pContext = &pThread->context;
	uint64_t tid = 0;
	if (thread_register(pThread, &tid)!=0){
		kfree((void*)pThread);
		virtualFree((uint64_t)pStack, stackSize);
		set_rflags(old_rflags);
		return -1;
	}
	uint64_t rflags = get_rflags()|(1<<9);
	pContext->rip = rip;
	pContext->rsp = rsp;
	pContext->rbp = 0x0;
	pContext->rcx = tid;
	pContext->rdx = argument;
	pContext->rflags = rflags;
	pThread->priority = THREAD_PRIORITY_NORMAL;
	pThread->tid = tid;
	pThread->start_rip = pContext->rip;
	*pTid = tid;
	set_rflags(old_rflags);
	return 0;
}
KAPI int thread_destroy(uint64_t tid){
	uint64_t old_rflags = get_rflags();
	__asm__ volatile("cli");
	uint64_t current_tid = 0;
	if (get_current_thread(&current_tid)!=0){
		set_rflags(old_rflags);
		return -1;
	}
	if (tid==current_tid&&!threadDestroySafe){
		printf("switching to safe stack\r\n");
		thread_destroy_safe(tid);
		printf("why are we here ???? - thread_destroy kernel/cpu/thread.c\r\n");
		while (1){};
	}
	threadDestroySafe = 0;
	struct thread_t* pThread = (struct thread_t*)0x0;
	if (subsystem_get_entry(pSubsystemDesc, tid, (uint64_t*)&pThread)!=0){
		set_rflags(old_rflags);
		return -1;
	}
	unsigned char* pLastStackGuard = (unsigned char*)(pThread->pStack+pThread->stackGuardSize+pThread->stackReserve);
	unsigned char* pRealStack = (unsigned char*)(pThread->pStack+pThread->stackGuardSize);
	uint64_t stackGuardPages = align_up(pThread->stackGuardSize, PAGE_SIZE)/PAGE_SIZE;
	if (virtualFree((uint64_t)pRealStack, pThread->stackReserve)!=0){
		kfree((void*)pThread);
		set_rflags(old_rflags);
		return -1;
	}
	if (virtualUnmapPages(pThread->pStack, stackGuardPages, 0)!=0){
		kfree((void*)pThread);
		set_rflags(old_rflags);
		return -1;
	}
	if (virtualUnmapPages((uint64_t)pLastStackGuard, stackGuardPages, 0)!=0){
		kfree((void*)pThread);
		set_rflags(old_rflags);
		return -1;
	}
	printf("unregistering thread\r\n");
	if (thread_unregister(tid)!=0){
		set_rflags(old_rflags);
		return -1;
	}
	kfree((void*)pThread);
	if (tid==current_tid){
		thread_yield();
		while (1){};
	}
	set_rflags(old_rflags);
	return 0;
}
KAPI int thread_get_status(uint64_t tid, uint64_t* pStatus){
	if (!pStatus)
		return -1;
	struct thread_t* pThread = (struct thread_t*)0x0;
	if (subsystem_get_entry(pSubsystemDesc, tid, (uint64_t*)&pThread)!=0)
		return -1;
	if (!pThread){
		return -1;
	}
	*pStatus = pThread->status;
	return 0;
}
KAPI int thread_set_status(uint64_t tid, uint64_t status){
	struct thread_t* pThread = (struct thread_t*)0x0;
	if (subsystem_get_entry(pSubsystemDesc, tid, (uint64_t*)&pThread)!=0)
		return -1;
	if (!pThread)
		return -1;
	pThread->status = status;
	return 0;
}
KAPI int thread_get_priority(uint64_t tid, uint64_t* pPriority){
	if (!pPriority)
		return -1;
	struct thread_t* pThread = (struct thread_t*)0x0;
	if (subsystem_get_entry(pSubsystemDesc, tid, (uint64_t*)&pThread)!=0)
		return -1;
	if (!pThread)
		return -1;
	*pPriority = pThread->priority;
	return 0;
}
KAPI int thread_set_priority(uint64_t tid, uint64_t priority){
	struct thread_t* pThread = (struct thread_t*)0x0;
	if (subsystem_get_entry(pSubsystemDesc, tid, (uint64_t*)&pThread)!=0)
		return -1;
	if (!pThread)
		return -1;
	pThread->priority = priority;
	return 0;
}
KAPI int thread_yield(void){
	if (!pFirstThread||!pCurrentThread)
		return -1;
	__asm__ volatile("sti");
	__asm__ volatile("int $0x30");
	return 0;
}
KAPI int get_current_thread(uint64_t* pTid){
	if (!pTid)
		return -1;
	*pTid = pCurrentThread->tid;
	return 0;
}
KAPI int thread_exists(uint64_t tid){
	struct thread_t* pThread = (struct thread_t*)0x0;
	if (subsystem_get_entry(pSubsystemDesc, tid, (uint64_t*)&pThread)!=0)
		return -1;
	if (!pThread)
		return -1;
	return 0;
}
