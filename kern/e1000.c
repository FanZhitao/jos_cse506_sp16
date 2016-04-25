#include <kern/e1000.h>
#include <kern/pmap.h>
#include <inc/stdio.h>

// LAB 6: Your driver code here

#define E1000_STATUS  		0x00008  /* Device Status - RO */
#define E1000_STATUS_FD         0x00000001      /* Full duplex.0=half,1=full */

// use 'volatile' to avoid cache and reorder access to this memory
volatile uint32_t *netipc;

int init_e1000(struct pci_func *pcif)
{
	// 1.Enable E1000 device
	pci_func_enable(pcif);

	cprintf("reg_base=%016llx, reg_size=%016llx\n", pcif->reg_base[0], pcif->reg_size[0]);
	
	// 2.Build memory map for BAR-0
	netipc = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);

	cprintf("FD Status: %08x\n", netipc[E1000_STATUS]);

	return 0;
}

