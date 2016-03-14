// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	
	pte_t pte;
	
	pte = uvpt[PGNUM(addr)];

	if (!((pte & PTE_W) && (pte & PTE_COW)))
		panic("Faulting address is not Write && COW page");

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.
	//   No need to explicitly delete the old page's mapping.

	// LAB 4: Your code here.

	envid_t curenvid;

	// '0' represents current env (handled in envid2env)
	curenvid = 0;

	// 1.Allocate a new page: pte => PFTEMP
	if ((r = sys_page_alloc(curenvid, 
			(void *) PFTEMP, PTE_P|PTE_U|PTE_W)) < 0)
		panic("allocating at %x in page fault handler: %e", addr, r);
	
	// 2.Copy content: [addr,addr+PGSIZE) => [PFTEMP,PFTEMP+PGSIZE)
	memmove((void *) PFTEMP, addr, PGSIZE);

	// 3.Change mapping: curenv pte of PFTEMP => curenv pte of addr
	if ((r = sys_page_map(curenvid, (void *) PFTEMP, curenvid, addr, PTE_P|PTE_U|PTE_W)) < 0)
		panic("sys_page_map: %e", r);
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.
	envid_t srcenvid, dstenvid;
	void *addr;
	pte_t pte;

	srcenvid = 0;
	dstenvid = envid;
	addr = (void *) (uintptr_t) (pn*PGSIZE);
	pte = uvpt[pn];

	if (((uintptr_t) addr) >= UTOP)
		panic("Address is above UTOP");

	if (!(pte & PTE_W) && !(pte & PTE_COW)) {
		// 1.Handle read-only page
		if (sys_page_map(srcenvid, addr, dstenvid, addr, PTE_P|PTE_U))
			panic("Failed to duppage non-writable/COW page");
	} else {
		// 2.1 Handle writable/COW page
		if ((r = sys_page_map(srcenvid, addr, dstenvid, addr, PTE_P|PTE_U|PTE_COW)) < 0)
			panic("sys_page_map: %e", r);

		// 2.2 Mark parent pte as COW as well
		if ((r = sys_page_map(srcenvid, addr, dstenvid, addr, PTE_P|PTE_U|PTE_COW)) < 0)
			panic("sys_page_map: %e", r);
	}
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	
	envid_t envid;
	uint8_t *addr;
	int r;
	int i, j, pn;
	extern unsigned char end[];

	// 1.Set pgfault() as page fault handler before fork
	set_pgfault_handler(pgfault);

	// 2.Allocate a new child environment.
	envid = sys_exofork();
	if (envid < 0)
		panic("sys_exofork: %e", envid);
	if (envid == 0) {
		// We're the child.
		// The copied value of the global variable 'thisenv'
		// is no longer valid (it refers to the parent!).
		// Fix it and return 0.
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}

	// 3.Only copy our address mapping into child, not page content.
	// 	UTEXT =0x800000 where program code located
	//for (addr = (uint8_t*) UTEXT; addr < end; addr += PGSIZE)
	//	duppage(envid, addr);
	//uintptr_t aaa = UTOP;
	//int bbb = VPD(UTOP);
	for (i = 0; i < VPD(end); i++) {
		if (!(uvpd[i] & PTE_P))
			continue;

		for (j = 0; j < 512; j++) {
			pn = i * 512 + j;
			if (!(uvpt[pn] & PTE_P))
				continue;
			
			// UX stack will be mapped later
			if (pn != PGNUM(UXSTACKTOP-PGSIZE))
				duppage(envid, pn);
		}
	}

	// 4.Also copy the stack we are currently running on.
	//duppage(envid, ROUNDDOWN(&addr, PGSIZE));

	// 5.Start the child environment running
	if ((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0)
		panic("sys_env_set_status: %e", r);

	return envid;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
