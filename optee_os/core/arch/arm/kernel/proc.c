/*
	2018-2-3
*/

#include <arm.h>
#include <sm/sm.h>
#include <kernel/thread.h>
#include <trace.h>
#include <kernel/spinlock.h>
#include <kernel/proc.h>

//TODO 2018-2-3
#include "thread_private.h"
//TODO 2018-2-12
#include <string.h>

#ifdef CFG_WITH_ARM_TRUSTED_FW
#define STACK_TMP_OFFS		0
#else
#define STACK_TMP_OFFS		SM_STACK_TMP_RESERVE_SIZE
#endif


#ifdef ARM32
#ifdef CFG_CORE_SANITIZE_KADDRESS
#define STACK_TMP_SIZE		(3072 + STACK_TMP_OFFS)
#else
#define STACK_TMP_SIZE		(1536 + STACK_TMP_OFFS)
#endif
#define STACK_THREAD_SIZE	8192

#ifdef CFG_CORE_SANITIZE_KADDRESS
#define STACK_ABT_SIZE		3072
#else
#define STACK_ABT_SIZE		2048
#endif

#endif /*ARM32*/

#ifdef ARM64
#define STACK_TMP_SIZE		(2048 + STACK_TMP_OFFS)
#define STACK_THREAD_SIZE	8192

#if TRACE_LEVEL > 0
#define STACK_ABT_SIZE		3072
#else
#define STACK_ABT_SIZE		1024
#endif
#endif /*ARM64*/

#ifdef CFG_WITH_STACK_CANARIES
#ifdef ARM32
#define STACK_CANARY_SIZE	(4 * sizeof(uint32_t))
#endif
#ifdef ARM64
#define STACK_CANARY_SIZE	(8 * sizeof(uint32_t))
#endif
#define START_CANARY_VALUE	0xdededede
#define END_CANARY_VALUE	0xabababab
#define GET_START_CANARY(name, stack_num) name[stack_num][0]
#define GET_END_CANARY(name, stack_num) \
	name[stack_num][sizeof(name[stack_num]) / sizeof(uint32_t) - 1]
#else
#define STACK_CANARY_SIZE	0
#endif

#define DECLARE_STACK(name, num_stacks, stack_size, linkage) \
linkage uint32_t name[num_stacks] \
		[ROUNDUP(stack_size + STACK_CANARY_SIZE, STACK_ALIGNMENT) / \
		sizeof(uint32_t)] \
		__attribute__((section(".nozi_stack"), \
			       aligned(STACK_ALIGNMENT)))

#define STACK_SIZE(stack) (sizeof(stack) - STACK_CANARY_SIZE / 2)

#define GET_STACK(stack) \
	((vaddr_t)(stack) + STACK_SIZE(stack))

struct proc procs[NUM_PROCS];

struct list_head run_queues[NUM_PRIO];

static unsigned int proc_global_lock = SPINLOCK_UNLOCK;

DECLARE_STACK(stack_proc, NUM_PROCS, STACK_THREAD_SIZE, static);

static void lock_global(void)
{
	cpu_spin_lock(&proc_global_lock);
}

static void unlock_global(void)
{
	cpu_spin_unlock(&proc_global_lock);
}

//TODO 2018-2-3
static void init_proc(void)
{
	int i;
	for(i = 0; i < NUM_PROCS; i++) {
		procs[i].k_stack = GET_STACK(stack_proc[i]);
		procs[i].uregs = (void*)(procs[i].k_stack - sizeof(struct pcb_regs));
		procs[i].p_endpoint = -1;
		SLIST_INIT(&procs[i].pgt_cache);
	}
	for(i = 0; i < NUM_PRIO; i++) {
		struct list_head *tp = &run_queues[i];
		run_queues[i].next = tp;
		run_queues[i].prev = tp;
	}
}


//TODO 2018-2-3
void proc_clr_boot(void)
{
	int res;
	init_cpu_locals();
	init_proc();
	res = proc_alloc((void*)0x6100000ul);
	if(res != 0)
	{
		DMSG("proc_alloc error!\n");
	}
	res = proc_alloc((void*)0x61226c4ul);
	if(res != 0)
	{
		DMSG("proc_alloc error!\n");
	}
	//TODO 2018-2-6
	proc_schedule();
}

//TODO 2018-2-3
int proc_alloc(void *ta)
{
	size_t n;
	bool found_proc = false;
	struct proc* proc;
	uint32_t spsr = SPSR_64(SPSR_64_MODE_EL1, SPSR_64_MODE_SP_EL0, 0);
	spsr |= read_daif();

	lock_global();

	for (n = 0; n < NUM_PROCS; n++) 
	{
		if (procs[n].p_endpoint == -1) 
		{
			procs[n].p_endpoint = n;
			found_proc = true;
			break;
		}
	}

	unlock_global();

	if (!found_proc) {
		DMSG("\nproc alloc error!\n");
		return -1;
	}

	proc = &procs[n];
	proc->regs.pc = (uint64_t)proc_load_entry;

	/*
	 * Stdcalls starts in SVC mode with masked foreign interrupts, masked
	 * Asynchronous abort and unmasked native interrupts.
	 */
	proc->regs.spsr = SPSR_64(SPSR_64_MODE_EL1, SPSR_64_MODE_SP_EL0,
				THREAD_EXCP_FOREIGN_INTR | DAIFBIT_ABT);

	/* Reinitialize stack pointer */
	proc->regs.sp = proc->k_stack;
	proc->p_prio = 4;
	/*
	 * Copy arguments into context. This will make the
	 * arguments appear in x0-x7 when thread is started.
	 */
	proc->regs.x[0] = (uint64_t)ta;
	proc->regs.x[1] = (uint64_t)n;
	/* Set up frame pointer as per the Aarch64 AAPCS */
	proc->regs.x[29] = 0;

	//thread_lazy_save_ns_vfp();
	return call_resume(&procs[n].regs, spsr);
}

//TODO 2018-2-6
void proc_schedule(void) 
{
	struct cpu_local *cpu_l = get_cpu_local();
	struct core_mmu_user_map map = { 0 };
	struct proc *proc = NULL;
	struct list_head* lh;
	int i;
	//lock_global();

	cpu_l->cur_proc = -1;
	for(i = 0; i < NUM_PRIO; i++) {
		lh = &run_queues[i]; 
		if(lh->next != lh) {
			proc = container_of(lh->next, struct proc, link);
			lh->next = lh->next->next;
			lh->next->prev = lh;
			break;
		}
	}
    //proc = proc_head.next;
	if(proc == NULL)
	{
		DMSG("proc_schedulde error\n");
		DMSG("no proc to run, cpu is going to idle\n");
		test_cpu_idle();
	}
	//DMSG("sn_sched proc %d\n", proc->p_endpoint);
    //proc->next->prev = &proc_head;
    //proc_head.next = proc->next;
	//unlock_global();
	assert(proc->p_endpoint >= 0);
	cpu_l->cur_proc = proc->p_endpoint;
	map.user_map = proc->map;
	core_mmu_set_user_map(&map);

	//message
	//TODO 2018-2-12
	if(proc->p_misc_flags & P_DELIVE) 
	{
		memcpy(proc->p_recvaddr, (void*)&proc->p_recvmsg, sizeof(struct message));
		proc->p_misc_flags &= (~P_DELIVE);
		//DMSG("test %s\n", (proc->p_recvmsg).msg);
	}
	//thread_lazy_save_ns_vfp();
	if(proc->p_misc_flags & P_INTER) 
	{
		proc->p_misc_flags &= (~P_INTER);
		proc_resume(&(proc->regs));
	} 
	else
	{
		proc_resume(proc->uregs);
	}
}

//TODO 2018-2-9
struct proc *get_proc(void)
{
	struct cpu_local *cpu_l = get_cpu_local();
	int cur = cpu_l->cur_proc;
	if(cur >= 0 && cur < NUM_PROCS && procs[cur].p_endpoint == cur)
	{
		return &procs[cur];
	}
	return NULL;
}

//TODO 2018-2-10
// insert tail
int enqueue(struct proc* p) 
{
	struct list_head *lh;
	struct list_head *lp;

	if(p == NULL || p->p_rts_flags != 0)
	{
		return -1;
	}
	lh = &run_queues[p->p_prio];
	lp = &p->link;
	lp->prev = lh->prev;
	lp->next = lh;
	lh->prev->next = lp;
	lh->prev = lp; 
	return 0;
}

//TODO 2018-2-10
// insert head
int enqueue_head(struct proc* p) 
{
	struct list_head *lh;
	struct list_head *lp;
	if(p == NULL || p->p_rts_flags != 0)
	{
		return -1;
	}

	lh = &run_queues[p->p_prio];
	lp = &p->link;
	lp->next = lh->next;
	lp->prev = lh;
	lh->next = lp;
	lp->next->prev = lp;
	return 0;
}

//TODO 2018-2-13
int proc_fork(struct proc *proc)
{
	size_t n;
	bool found_proc = false;
	struct proc* p_ch;
	TEE_Result res;
	int mp_id = proc->uregs->x[0];

	if(mp_id < 0 || procs[mp_id].p_endpoint != mp_id)
	{
		return -1;
	}
	proc = &procs[mp_id];

	lock_global();
	for (n = 0; n < NUM_PROCS; n++) 
	{
		if (procs[n].p_endpoint == -1) 
		{
			procs[n].p_endpoint = n;
			found_proc = true;
			break;
		}
	}
	unlock_global();

	if (!found_proc) 
	{
		DMSG("\nproc alloc error!\n");
		return -1;
	}

	p_ch = &procs[n];

	p_ch->p_prio = proc->p_prio;
	*(p_ch->uregs) = *(proc->uregs);
	p_ch->p_rts_flags = proc->p_rts_flags;
	p_ch->p_getfrom = proc->p_getfrom;
	p_ch->p_recvaddr = proc->p_recvaddr;
	res = ta_copy(proc, p_ch);
	if(res != TEE_SUCCESS) 
	{
		DMSG("\nta_copy error!\n");
		return -1;
	}
	return n;
}

//TODO 2018-2-16
void __noreturn test_cpu_idle(void)
{
	int num = 0;
	while(1)
	{
		if(num % (60 * 1024 * 1024) == 1)
		{
			DMSG("cpu idle...");
		}
		num++;
	}
}

//TODO 2018-2-17
int proc_get_id(void)
{
	int ct;
	struct cpu_local *l = get_cpu_local();
	uint32_t exceptions = thread_mask_exceptions(THREAD_EXCP_FOREIGN_INTR);
	ct = l->cur_proc;
	thread_unmask_exceptions(exceptions);
	assert(ct >= 0 && ct < NUM_PROCS);
	return ct;
}

//TODO 2018-2-17
void proc_add_mutex(struct mutex *m)
{
	struct cpu_local *l = get_cpu_local();
	int ct = l->cur_proc;

	assert(ct != -1 && procs[ct].p_rts_flags == 0);
	assert(m->owner_id == MUTEX_OWNER_ID_NONE);
	m->owner_id = ct;
	TAILQ_INSERT_TAIL(&procs[ct].mutexes, m, link);
}

//TODO 2018-2-17
void mutex_proc_sleep(void) 
{
	__proc_suspend();
}

//TODO 2018-2-17
void proc_state_suspend(void)
{
	/*
	struct cpu_local *l = get_cpu_local();
	int ct = l->cur_proc;
	struct proc *p;
	assert(ct != -1);
	p = &procs[ct];
	lock_global();
	assert(threads[ct].state == THREAD_STATE_ACTIVE);
	threads[ct].have_user_map = core_mmu_user_mapping_is_active();
	if (threads[ct].have_user_map) {
		core_mmu_get_user_map(&threads[ct].user_map);
		core_mmu_set_user_map(NULL);
	}
	unlock_global();
	*/
	proc_schedule();
}

//TODO 2018-2-17
void proc_rem_mutex(struct mutex *m)
{
	struct cpu_local *l = get_cpu_local();
	int ct = l->cur_proc;

	assert(ct != -1 && procs[ct].p_rts_flags == 0);
	assert(m->owner_id == ct);
	m->owner_id = MUTEX_OWNER_ID_NONE;
	TAILQ_REMOVE(&procs[ct].mutexes, m, link);
}
