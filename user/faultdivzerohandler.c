// test user-level fault handler -- just exit when we fault

#include <inc/lib.h>

void
handler(struct UTrapframe *utf)
{
	void *addr = (void*)utf->utf_rip;
	cprintf("divide zero error at va %x\n", addr);
	sys_env_destroy(sys_getenvid());
}

int zero = 0;

void
umain(int argc, char **argv)
{
	set_pgfault_handler(handler);
	cprintf("1/0 is %08x!\n", 1/zero);
}
