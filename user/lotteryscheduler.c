#include <inc/lib.h>

volatile bool stop = 0;

void
umain(int argc, char **argv)
{
	int i, j;
	int seen;
	int assign_tickets[3] = { 100, 200, 400 };
	envid_t parent = sys_getenvid();

	// sfork 3 'threads' with tickets 100, 200 and 500
	for (i = 0; i < 3; i++) {
		if (sfork() == 0) {
			sys_set_priority(assign_tickets[i]);
			break;
		}
	}

	if (i == 3) {
		sys_yield();
		return;
	}

	// Wait for the parent to finish forking
	while (envs[ENVX(parent)].env_status != ENV_FREE)
		asm volatile("pause");

	// Check that one environment doesn't run on two CPUs at once
	int counter = 0;
	for (i = 0; i < 1000*1000*100; i++) {
		if (stop)
			break;
		counter++;
	}

	stop = 1;

	// env_runs does not work. It increment 1 only when context switch.
	cprintf("\n[%08x] env runs: %d\n\n", thisenv->env_id, thisenv->env_runs);
}

