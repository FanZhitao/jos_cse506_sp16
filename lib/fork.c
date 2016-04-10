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
	
	//cprintf("[%08x] Start to Copy-On-Write on [0x%x]\n", thisenv->env_id, addr);

	pte = uvpt[PGNUM(addr)];

	// 1.Fault is caused by write and page is COW
	if (!((err & FEC_WR) && (pte & PTE_COW)))
		panic("[%08x] Faulting address [%x] is not caused by write && COW page: rip=[0x%x], pte=[%x], err=[%d]", thisenv->env_id, addr, utf->utf_rip, pte, err);

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.
	//   No need to explicitly delete the old page's mapping.

	// LAB 4: Your code here.

	envid_t curenvid;
	void *va;

	// '0' represents current env (handled in envid2env)
	curenvid = 0;

	// 2.Allocate a new page: pte => PFTEMP
	if ((r = sys_page_alloc(curenvid, 
			(void *) PFTEMP, PTE_P|PTE_U|PTE_W)) < 0)
		panic("allocating at 0x%x in page fault handler: %e", addr, r);

	va = (void *) ROUNDDOWN(addr, PGSIZE);

	// 3.Copy page content: [va, va+PGSIZE) => [PFTEMP,PFTEMP+PGSIZE)
	memmove((void *) PFTEMP, va, PGSIZE);

	// 4.Change mapping: curenv pte of PFTEMP => curenv pte of va
	if ((r = sys_page_map(curenvid, (void *) PFTEMP, curenvid, va, PTE_P|PTE_U|PTE_W)) < 0)
		panic("sys_page_map: %e", r);

	//cprintf("[%08x] Copy-On-Write on [0x%x] complete\n", thisenv->env_id, addr);
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

	if (!((pte & PTE_W) || (pte & PTE_COW))) {
		// 1.Handle read-only page
		if (sys_page_map(srcenvid, addr, dstenvid, addr, PTE_P|PTE_U))
			panic("Failed to duppage non-writable/COW page");
		//cprintf("duppage Readonly page at %x\n, addr");
	} else {
		//cprintf("[%08x-->%08x] duppage Writable/COW page at [0x%x]\n", thisenv->env_id, dstenvid, addr);
		// 2.1 Handle writable/COW page
		if ((r = sys_page_map(srcenvid, addr, dstenvid, addr, PTE_P|PTE_U|PTE_COW)) < 0)
			panic("sys_page_map: %e", r);

		// 2.2 Mark parent pte as COW as well
		if ((r = sys_page_map(srcenvid, addr, srcenvid, addr, PTE_P|PTE_U|PTE_COW)) < 0)
			panic("sys_page_map: %e", r);
	}
	return 0;
}

static int
duppage2(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.
	envid_t srcenvid, dstenvid;
	void *addr;

	srcenvid = 0;
	dstenvid = envid;
	addr = (void *) (uintptr_t) (pn*PGSIZE);

	if (((uintptr_t) addr) >= UTOP)
		panic("Address is above UTOP");

	cprintf("[%08x-->%08x] duppage2 page at [0x%x]\n", thisenv->env_id, dstenvid, addr);

	// 1.Handle read-only page
	if (sys_page_map(srcenvid, addr, dstenvid, addr, PTE_P|PTE_U|PTE_W))
		panic("Failed to duppage2 non-writable/COW page");
	return 0;
}

const uint32_t CLONE_NO = 0;
const uint32_t CLONE_VM = 1;

envid_t
clone(uint32_t flag)
{
	envid_t envid;
	uintptr_t va;
	int r;

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
		// NOTE: for lab 4 - challenge 6 multi-threading
		//thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}

	/** 
	 * 3.Only copy our address mapping into child rather than page content.
	 * Could make it by iterating page table instead of va:
	 *
	 * for (i = 0; i < NPMLENTRIES; i++) {
 	 *   pn = i;
	 *   if (!(uvpml4e[pn] & PTE_P)) continue;
	 *
	 *   for (j = 0; j < NPDPENTRIES; j++) {
 	 *     pn = i*NPMLENTRIES + j;
	 *     if (!(uvpde[pn] & PTE_P)) continue;
	 *
	 *     for (k = 0; k < NPDENTRIES; k++) {
 	 *       pn = i*NPMLENTRIES*NPMLENTRIES + j*NPDPENTRIES + k;
	 *       if (!(uvpd[pn] & PTE_P)) continue;
	 *
	 *       for (m = 0; m < NPTENTRIES; m++) {
 	 *         pn = i*NPMLENTRIES*NPMLENTRIES*NPMLENTRIES + j*NPDPENTRIES*NPDPENTRIES + k*NPDENTRIES + m;
 	 *         if (!(uvpt[pn] & PTE_P)) continue;
	 * 		......
	 *
	 * NOTE: touch uvpt[m] will cause page fault if uvpd[k] is NOT present.
	 */
	//cprintf("\n[%08x] Start to copy page table\n", thisenv->env_id);
	for (va = 0; va < UTOP; va+=PGSIZE) {

		// Skip to speed up copy to avoid timeout in forktree test
		if (!(uvpml4e[VPML4E(va)] & PTE_P)) { 
			va += ((((uintptr_t) 1) << PML4SHIFT) - PGSIZE);
			continue;
		}

		if (!(uvpde[VPDPE(va)] & PTE_P)) {
			va += ((((uintptr_t) 1) << PDPESHIFT) - PGSIZE);
			continue;
		}

		if (!(uvpd[VPD(va)] & PTE_P)) {
			va += ((((uintptr_t) 1) << PDXSHIFT) - PGSIZE);
			continue;
		}

		if (!(uvpt[VPN(va)] & PTE_P))
			continue;

		if (va == (UXSTACKTOP-PGSIZE))
			continue;

		if ((flag & CLONE_VM) && (va != (USTACKTOP-PGSIZE))) {
			if ((r = duppage2(envid, PGNUM(va))) < 0)
				panic("duppage2 at [0x%x]: %e", va, r);
		// Lab 5: share fd state if pte is PTE_SHARE
		} else if ((flag & CLONE_NO) && (uvpt[VPN(va)] & PTE_SHARE)) {
			if ((r = duppage2(envid, PGNUM(va))) < 0)
				panic("duppage2 at [0x%x]: %e", va, r);
		} else {
			if ((r = duppage(envid, PGNUM(va))) < 0)
				panic("duppage at [0x%x]: %e", va, r);
		}
	}
	//cprintf("[%08x] Copy page table complete\n\n", thisenv->env_id);
	
	// 4.Allocate new exception stack for child
	if ((r = sys_page_alloc(envid, (void *) (UXSTACKTOP-PGSIZE), PTE_P|PTE_U|PTE_W)) < 0)
		panic("sys_page_alloc: %e", r);

	// 5.Copy page fault handler setup
	//  set_pgfault_handler() use curenv and upcall wrapper as default
	//  so we call underlying syscall with wrapper instead
	if ((r = sys_env_set_pgfault_upcall(envid, thisenv->env_pgfault_upcall)) < 0)
		panic("sys_env_set_pgfault_upcall: %e", r);

	// 6.Start the child environment running
	if ((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0)
		panic("sys_env_set_status: %e", r);

	return envid;
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
	return clone(CLONE_NO);
}

// Lab 4 - Challenge 6!
int
sfork(void)
{
	return clone(CLONE_VM);
}
