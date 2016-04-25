#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	envid_t pid;
	int r;

	if ((pid = fork()) < 0)
		panic("Failed to fork: %d\n", pid);

	if (pid == 0) {
		cprintf("i am child environment %08x\n", thisenv->env_id);

		if ((r = exec("/bin/hello", "hello", 0)) < 0)
			panic("spawn(hello) failed: %e", r);
	} else {
		cprintf("i am parent environment %08x\n", thisenv->env_id);
	}
}
