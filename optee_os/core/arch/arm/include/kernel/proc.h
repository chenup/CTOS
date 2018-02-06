/*
	2018-2-2
*/
#ifndef KERNEL_PROC_H
#define KERNEL_PROC_H

#include <rtos_list.h>
#include <mm/tee_mmu_types.h>
#include <kernel/ipc.h>
#include <kernel/user_ta.h>

#define NUM_PROCS 16
#define NUM_PRIO 8

struct cpu_local {
	vaddr_t tmp_stack;
	int cur_proc;
	uint64_t x[4];
};

struct pcb_regs {
    uint64_t sp;
    uint64_t pc;
    uint64_t spsr;
    uint64_t x[31];
};

struct proc {
    struct pcb_regs regs;
    struct pcb_regs *uregs;
    uint64_t map;
    struct pgt_cache pgt_cache;
    uint32_t p_num;
    int p_endpoint;
    uint32_t time_res;
    uint32_t p_prio;
    uint32_t p_priv;
	uint32_t p_rts_flags;
	uint32_t p_misc_flags;
	struct message p_sendmsg;
	int p_sendto;	
	struct message p_recvmsg;	
	int p_getfrom;
	void* p_recvaddr;	
    struct run_info run_info;
    uint64_t k_stack;
	struct proc* p_caller_q;
	struct proc* p_q_link;
	struct list_head link;
};

//TODO 2018-2-3
void proc_clr_boot(void);

//TODO 2018-2-3
int proc_alloc_and_run(void *ta);

#endif