#include <inc/assert.h>
#include <inc/x86.h>
#include <kern/spinlock.h>
#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/monitor.h>

void sched_halt(void);

void
sched_yield_roundrobin(void)
{
	// Implement simple round-robin scheduling.
	//
	// Search through 'envs' for an ENV_RUNNABLE environment in
	// circular fashion starting just after the env this CPU was
	// last running.  Switch to the first such environment found.
	//
	// If no envs are runnable, but the environment previously
	// running on this CPU is still ENV_RUNNING, it's okay to
	// choose that environment.
	//
	// Never choose an environment that's currently running on
	// another CPU (env_status == ENV_RUNNING). If there are
	// no runnable environments, simply drop through to the code
	// below to halt the cpu.

	// LAB 4: Your code here.
	int i = 0; // ENVX(eid) is 10 bit
	struct Env *e = 0;

	// If there was a previously running environment, picks it as first
	if (curenv != 0)
		i = (curenv->env_id + 1) % NENV;	
	
	int end_of_i = i == 0 ? NENV - 1 : i - 1;
	for (; i != end_of_i; i = (i + 1) % NENV)
	{
		struct Env *this_e = &envs[i];
		if (this_e->env_status == ENV_RUNNABLE)
		{
			e = this_e;
			break;
		}
	}

	if (e == 0 && curenv!= 0 && curenv->env_status == ENV_RUNNING && curenv->env_cpunum == cpunum())
		e = curenv;

	if (e != 0)
	{
		//cprintf("sched_yield_roundrobin() pick [%08x] env to run\n", e->env_id);
		env_run(e);
	}
}

// Choose a user environment to run and run it.
void
sched_yield(void)
{
	struct Env *idle;

	sched_yield_roundrobin();

	// sched_halt never returns
	sched_halt();
}

// Halt this CPU when there is nothing to do. Wait until the
// timer interrupt wakes it up. This function never returns.
//
void
sched_halt(void)
{
	int i;

	// For debugging and testing purposes, if there are no runnable
	// environments in the system, then drop into the kernel monitor.
	for (i = 0; i < NENV; i++) {
		if ((envs[i].env_status == ENV_RUNNABLE ||
		     envs[i].env_status == ENV_RUNNING ||
		     envs[i].env_status == ENV_DYING))
			break;
	}
	if (i == NENV) {
		cprintf("No runnable environments in the system!\n");
		while (1)
			monitor(NULL);
	}

	// Mark that no environment is running on this CPU
	curenv = NULL;
	lcr3(PADDR(boot_pml4e));

	// Mark that this CPU is in the HALT state, so that when
	// timer interupts come in, we know we should re-acquire the
	// big kernel lock
	xchg(&thiscpu->cpu_status, CPU_HALTED);

	// Release the big kernel lock as if we were "leaving" the kernel
	unlock_kernel();

	// Reset stack pointer, enable interrupts and then halt.
	asm volatile (
		"movq $0, %%rbp\n"
		"movq %0, %%rsp\n"
		"pushq $0\n"
		"pushq $0\n"
		"sti\n"
		"hlt\n"
		: : "a" (thiscpu->cpu_ts.ts_esp0));
}

