#include <kern/e1000.h>
#include <kern/pmap.h>
#include <inc/stdio.h>
#include <inc/string.h>
#include <kern/pmap.h>
#include <kern/env.h>

// LAB 6: Your driver code here

// use 'volatile' to avoid cache and reorder access to this memory
volatile uint32_t *netipc;

/*
 * Since TDLEN must be 128-byte aligned and each transmit descriptor is 16 bytes, 
 * your transmit descriptor array will need some multiple of 8 transmit descriptors. 
 * However, don't use more than 64 descriptors or our tests won't be able to test transmit ring overflow.
 */ 
struct tx_desc tx_ring[MAX_TX_NUM];

struct pkt_buf pkt_bufs[MAX_TX_NUM];

void read_mac_addr(void)
{
	netipc[E1000_EECD] |= E1000_EECD_REQ;

	// Wait for hardware complete accessing
	while (!(netipc[E1000_EECD] & E1000_EECD_GNT));

	cprintf("Mac addr: %08x-%08x-%08x\n", 
			(uint16_t) netipc[E1000_EECD],
			(uint16_t) (netipc[E1000_EECD]>>16),
			(uint16_t) (netipc[E1000_EECD] + 1));

	netipc[E1000_EECD] &= ~E1000_EECD_REQ;
}

static void init_e1000_transmit(void)
{
	int i;

	// 1.Initialize buffer ring
	memset(tx_ring, 0x0, MAX_TX_NUM * sizeof(struct tx_desc));
	memset(pkt_bufs, 0x0, MAX_TX_NUM * sizeof(struct pkt_buf));
	for (i = 0; i < MAX_TX_NUM; i++) {
		tx_ring[i].addr = (uint64_t) PADDR(pkt_bufs + i);
		tx_ring[i].status |= E1000_TXD_STAT_DD;
	}
	netipc[E1000_TDBAL] = (uint64_t) PADDR(tx_ring);
	netipc[E1000_TDBAH] = ((uint64_t) PADDR(tx_ring))>>32;
	netipc[E1000_TDLEN] = MAX_TX_NUM * sizeof(struct tx_desc);
	netipc[E1000_TDH] = 0x0;
	netipc[E1000_TDT] = 0x0;
	cprintf("TDBAL: %016llx, TDBAH: %016llx\n", netipc[E1000_TDBAL], netipc[E1000_TDBAH]);

	// 2.Initialize Transmit Control Register
	netipc[E1000_TCTL] |= E1000_TCTL_EN; 
	netipc[E1000_TCTL] |= E1000_TCTL_PSP; 
	netipc[E1000_TCTL] &= ~E1000_TCTL_CT; 
	netipc[E1000_TCTL] |= (0x10<<4); 
	netipc[E1000_TCTL] &= ~E1000_TCTL_COLD; 
	netipc[E1000_TCTL] |= (0x40<<12); 
	cprintf("TCTL: %016llx\n", netipc[E1000_TCTL]);

	// NOTE: According to the IEEE802.3 standard, IPGR1 should be 2/3 of IPGR2 value.
	netipc[E1000_TIPG] |= (0x6<<20) | (0x4<<10) | (0xa<<0);
	cprintf("TIPG: %016llx\n", netipc[E1000_TIPG]);
}

static void init_e1000_receive(void)
{

}

int init_e1000(struct pci_func *pcif)
{
	// 1.Enable E1000 device
	pci_func_enable(pcif);
	cprintf("reg_base=%016llx, reg_size=%016llx\n", pcif->reg_base[0], pcif->reg_size[0]);
	
	// 2.Build memory map for BAR-0
	netipc = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);
	cprintf("FD Status: %08x\n", netipc[E1000_STATUS]);

	read_mac_addr();

	// 3.Initialize Transmit Ring Buffer and relevant registers
	init_e1000_transmit();

	// 4.Initialize Transmit Ring Buffer and relevant registers
	init_e1000_receive();

	return 0;
}

int send_packet_direct(void *packet, size_t len)
{
	struct tx_desc *tx;
	struct PageInfo *srcpp;
	pte_t *pte;

	// 1.Check if ring buffer is full
	tx = &tx_ring[netipc[E1000_TDT]];
	if (!(tx->status & E1000_TXD_STAT_DD)) {
		cprintf("Transmitting ring buffer is full\n");
		return -1;
	}
	tx->status &= ~E1000_TXD_STAT_DD;

	// 2.Copy buffer and update TDT
	//memcpy((void *) KADDR(tx->addr), packet, len); 
	
	/*
	 * Challenge 2: Zero-copy transmit
	 * 	Rather than copy, use user-space buffer directly, which is already mapped in OUTPUT env during IPC.
	 * 	So try to get its corresponding physical address with OUTPUT page table, and pass it to network device.
	 */
	if (!(srcpp = page_lookup(curenv->env_pml4e, packet, &pte)))
		return -1;
	tx->addr = page2pa(srcpp) + PGOFF(packet);
	tx->length = len;
	//cprintf("Zero-copy buffer packet=%016llx, addr=%016llx, len=%d\n", 
	//		packet, tx->addr, tx->length);

	// 3.Set RS bit to make NIC report status when send complete
	tx->cmd |= (E1000_TXD_CMD_RS | E1000_TXD_CMD_EOP);

	netipc[E1000_TDT] = (netipc[E1000_TDT] + 1) % MAX_TX_NUM;

	return 0;
}

int send_packet(void *packet, size_t len)
{
	struct tx_desc *tx;

	// 1.Check if ring buffer is full
	tx = &tx_ring[netipc[E1000_TDT]];
	if (!(tx->status & E1000_TXD_STAT_DD)) {
		cprintf("Transmitting ring buffer is full\n");
		return -1;
	}
	tx->status &= ~E1000_TXD_STAT_DD;

	// 2.Copy buffer and update TDT
	memcpy((void *) KADDR(tx->addr), packet, len); 
	tx->length = len;

	// 3.Set RS bit to make NIC report status when send complete
	tx->cmd |= (E1000_TXD_CMD_RS | E1000_TXD_CMD_EOP);

	netipc[E1000_TDT] = (netipc[E1000_TDT] + 1) % MAX_TX_NUM;

	return 0;
}

