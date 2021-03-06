// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/dwarf.h>
#include <kern/kdebug.h>
#include <kern/dwarf_api.h>
#include <kern/trap.h>
#include <kern/env.h>
#include <kern/pmap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "showmappings", "Display virtual and physical mapping", mon_showmappings },
	{ "si", "Single-step one instruction", mon_singlestep },
	{ "continue", "Continue execution", mon_continue },
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	// Your code here.
	uint64_t rbp;
        uintptr_t rip;
        struct Ripdebuginfo info, *infop = &info;
        int i, offset;

        cprintf("Stack backtrace:\n");
        read_rip(rip);
        for (rbp = read_rbp(); rbp != 0x0;
                rip = *((uintptr_t *) (rbp + 8)), rbp = *((uint64_t *) rbp)) {

                cprintf("  rbp %016llx  rip %016llx\n", rbp, rip);

                if (debuginfo_rip(rip, infop) == -1) {
                        panic("Cannot get debug info");
                }

                cprintf("       %s:%d: %s+%016llx  args:%d ",
                        infop->rip_file, infop->rip_line,
                        infop->rip_fn_name, (rip - infop->rip_fn_addr),
                        infop->rip_fn_narg
                );

		// assume all args are 4 bytes
                for (i = 0, offset = 0; i < infop->rip_fn_narg; i++) {
			offset += infop->size_fn_arg[i];
                        cprintf(" %016llx", *((int32_t *) (rbp - offset)));
                }
                cprintf("\n");
        }
        return 0;
}


uintptr_t atoi(char *str)
{
	uintptr_t i;

	// Assmue first 2 char are '0x'
	// Note: str is in hex
	i = 0;
	for (str += 2; *str; str++) {
		i = i * 16 + (uint64_t) (*str - '0');
	}
	return i;
}

int
mon_showmappings(int argc, char **argv, struct Trapframe *tf)
{
	uintptr_t startva, endva;
	pte_t *pte;

	startva = atoi(argv[1]);
	endva = atoi(argv[2]);

	if (startva > endva) {
		cprintf("Invalid input 0x%x ~ 0x%x:\n", startva, endva);
		return -1;
	}

	for (; startva <= endva; startva += PGSIZE) {
		pte = pml4e_walk(boot_pml4e, (void *) startva, 0);	

		if (pte) {
			cprintf("0x%x ===> 0x%x (%s)\n", 
				startva, 
				PTE_ADDR(*pte),
				(*pte & (PTE_W | PTE_U)) ? "PTE_W | PTE_U | PTE_P" :
					(*pte & PTE_W) ? "PTE_W | PTE_P" :
						(*pte & PTE_U) ? "PTE_U | PTE_P" :
							(*pte & PTE_P) ? "PTE_P" : "Not Present");
		} else {
			cprintf("0x%x ===> 0x%x (%s)\n", 
				startva, 
				0,
				"Not Present");
		}
	}
	return 0;
}

int
mon_singlestep(int argc, char **argv, struct Trapframe *tf)
{
	uint64_t eflags;

	cprintf("Current position: %016llx \n", tf->tf_rip);
	
	// Set eflags in tf, nor eflag register
	tf->tf_eflags |= FL_TF;

	// Breakpoint from user mode, curenv should be there in running state 
	if ((tf->tf_cs & 3) == 3) {
		assert(curenv && curenv->env_status == ENV_RUNNING);
		env_run(curenv);
	} else {
		panic("Breakpoint from kernel");
	}
	
	return 0;
}


int
mon_continue(int argc, char **argv, struct Trapframe *tf)
{
	uint64_t eflags;

	cprintf("Continue execution");

	// Reset eflags's TF bit
	if (tf->tf_eflags & FL_TF)
		tf->tf_eflags &= ~FL_TF;

	// Breakpoint from user mode, curenv should be there in running state 
	if ((tf->tf_cs & 3) == 3) {
		assert(curenv && curenv->env_status == ENV_RUNNING);
		env_run(curenv);
	} else {
		panic("Breakpoint from kernel");
	}
	
	return 0;
}


/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < NCOMMANDS; i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");

	// Print trapframe only if trapped from breakpoint
	//  and NOT in single-step mode
	if (tf != NULL && !(tf->tf_eflags & FL_TF))
		print_trapframe(tf);

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
