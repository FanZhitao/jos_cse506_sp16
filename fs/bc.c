
#include "fs.h"

// Return the virtual address of this disk block.
void*
diskaddr(uint64_t blockno)
{
	if (blockno == 0 || (super && blockno >= super->s_nblocks))
		panic("bad block number %08x in diskaddr", blockno);
	return (char*) (DISKMAP + blockno * BLKSIZE);
}

// Is this virtual address mapped?
bool
va_is_mapped(void *va)
{
	return (uvpml4e[VPML4E(va)] & PTE_P) && (uvpde[VPDPE(va)] & PTE_P) && (uvpd[VPD(va)] & PTE_P) && (uvpt[PGNUM(va)] & PTE_P);
}

// Is this virtual address dirty?
bool
va_is_dirty(void *va)
{
	return (uvpt[PGNUM(va)] & PTE_D) != 0;
}

// -------------------------------------------------------------
// Lab 5 - Challenge 2: Eviction of Block Cache

#define CACHE_THRESHOLD 10

struct Usage {
	uint64_t active[CACHE_THRESHOLD];
	uint32_t head, tail;
} usage = { .head = 0, .tail = 0 };

static void
bc_reclaim(uint64_t blockno)
{
	int i;
	void *va;

	// 0.Bypass bootsector, superblock, bitmap, ibitmap, 2*itable
	if (blockno <= 5)
		return;

	// 1.Bookkeep block usage
	//cprintf(" Pgfault block: %d\n", blockno);
	usage.active[usage.tail] = blockno;
	usage.tail = (usage.tail + 1) % CACHE_THRESHOLD;

	// 2.Check if cache memory is above threshold
	if (usage.head == usage.tail) {

		// 3.Second-Chance replacement algorithm
		// 3.1 Look for a victim
		for (i = usage.head; i <= usage.tail; i = (i+1) % CACHE_THRESHOLD) {
			va = diskaddr(usage.active[i]);
			if (uvpt[PGNUM(va)] & PTE_A) {
				// Restore "accessed" bit and give it a second chance
				sys_page_map(0, va, 0, va, uvpt[PGNUM(va)] & ~PTE_A);
				continue;
			}
		}

		// 3.2 All blocks are accessed, then just reclaim the head
		if (i == (usage.tail + 1) % CACHE_THRESHOLD) {
			i = usage.head;
			va = diskaddr(usage.active[i]);
		}

		// 3.3 Perform reclaimation
		//cprintf(" Reclaim block: %d\n", usage.active[i]);
		flush_block(va);
		sys_page_unmap(0, va);

		// 3.4 Move head pointer
		usage.head = (usage.head + 1) % CACHE_THRESHOLD;
	}
}
// -------------------------------------------------------------

// Fault any disk block that is read in to memory by
// loading it from disk.
// Hint: Use ide_read and BLKSECTS.
static void
bc_pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint64_t blockno = ((uint64_t)addr - DISKMAP) / BLKSIZE;
	int r;

	// Check that the fault was within the block cache region
	if (addr < (void*)DISKMAP || addr >= (void*)(DISKMAP + DISKSIZE))
		panic("page fault in FS: eip %08x, va %08x, err %04x",
		      utf->utf_rip, addr, utf->utf_err);

	// Sanity check the block number.
	if (super && blockno >= super->s_nblocks)
		panic("reading non-existent block %08x\n", blockno);

	// Lab 5 - Challenge 2
	bc_reclaim(blockno);

	// Allocate a page in the disk map region, read the contents
	// of the block from the disk into that page.
	// Hint: first round addr to page boundary.
	//
	// LAB 5: your code here:
	void *addr_aligned = ROUNDDOWN(addr, BLKSIZE);
	uint32_t secno = ((uint64_t)addr_aligned - DISKMAP) / SECTSIZE;
	if (sys_page_alloc(0, addr_aligned, PTE_SYSCALL) != 0)
		panic("sys_page_alloc failed at %08x\n", addr_aligned);
	if (ide_read(secno, addr_aligned, BLKSECTS) != 0)
		panic("reading disk failed at sector %d\n", secno); 

	if ((r = sys_page_map(0, addr_aligned, 0, addr_aligned, uvpt[PGNUM(addr_aligned)] & PTE_SYSCALL)) < 0)
		panic("in bc_pgfault, sys_page_map: %e", r);

	// Check that the block we read was allocated. (exercise for
	// the reader: why do we do this *after* reading the block
	// in?)
	if (bitmap && block_is_free(blockno))
		panic("reading free block %08x\n", blockno);
}

// Flush the contents of the block containing VA out to disk if
// necessary, then clear the PTE_D bit using sys_page_map.
// If the block is not in the block cache or is not dirty, does
// nothing.
// Hint: Use va_is_mapped, va_is_dirty, and ide_write.
// Hint: Use the PTE_SYSCALL constant when calling sys_page_map.
// Hint: Don't forget to round addr down.
void
flush_block(void *addr)
{
	uint64_t blockno = ((uint64_t)addr - DISKMAP) / BLKSIZE;

	if (addr < (void*)DISKMAP || addr >= (void*)(DISKMAP + DISKSIZE))
		panic("flush_block of bad va %08x", addr);

	// LAB 5: Your code here.
	void *addr_aligned = ROUNDDOWN(addr, BLKSIZE);
	//cprintf("flush block at %08x\n", addr_aligned);
	if (!va_is_mapped(addr_aligned) || !va_is_dirty(addr_aligned))
		return;
	uint32_t secno = ((uint64_t)addr_aligned - DISKMAP)/ SECTSIZE;
	//cprintf("write at sector %d\n", secno);
	if (ide_write(secno, addr_aligned, BLKSECTS) != 0)
		panic("writing disk failed at sector %d\n", secno);	
	if (sys_page_map(0, addr_aligned, 0, addr_aligned, uvpt[PGNUM(addr_aligned)] & PTE_SYSCALL) != 0)
		panic("sys_page_map call error in flush_block.");	
}

// Test that the block cache works, by smashing the superblock and
// reading it back.
static void
check_bc(void)
{
	struct Super backup;

	// back up super block
	memmove(&backup, diskaddr(1), sizeof backup);

	// smash it
	strcpy(diskaddr(1), "OOPS!\n");
	flush_block(diskaddr(1));
	assert(va_is_mapped(diskaddr(1)));
	assert(!va_is_dirty(diskaddr(1)));

	// clear it out
	sys_page_unmap(0, diskaddr(1));
	assert(!va_is_mapped(diskaddr(1)));

	// read it back in
	assert(strcmp(diskaddr(1), "OOPS!\n") == 0);

	// fix it
	memmove(diskaddr(1), &backup, sizeof backup);
	flush_block(diskaddr(1));

	cprintf("block cache is good\n");
}

void
bc_init(void)
{
	struct Super super;
	set_pgfault_handler(bc_pgfault);
	check_bc();

	// cache the super block by reading it once
	memmove(&super, diskaddr(1), sizeof super);
}

