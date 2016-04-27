#include <kern/e1000.h>
#include <kern/pmap.h>
#include <inc/stdio.h>
#include <inc/string.h>
#include <kern/pmap.h>

// LAB 6: Your driver code here

#define E1000_STATUS 	(0x00008/4)  /* Device Status - RO */
#define E1000_TCTL 	(0x00400/4)  /* TX Control - RW */
#define E1000_TIPG 	(0x00410/4)  /* TX Inter-packet gap -RW */
#define E1000_TDBAL 	(0x03800/4)  /* TX Descriptor Base Address Low - RW */
#define E1000_TDBAH 	(0x03804/4)  /* TX Descriptor Base Address High - RW */
#define E1000_TDLEN 	(0x03808/4)  /* TX Descriptor Length - RW */
#define E1000_TDH 	(0x03810/4)  /* TX Descriptor Head - RW */
#define E1000_TDT 	(0x03818/4)  /* TX Descripotr Tail - RW */

#define E1000_STATUS_FD	0x00000001    /* Full duplex.0=half,1=full */

#define E1000_TCTL_EN 	0x00000002    /* enable tx */
#define E1000_TCTL_PSP 	0x00000008    /* pad short packets */
#define E1000_TCTL_CT 	0x00000ff0    /* collision threshold */
#define E1000_TCTL_COLD 0x003ff000    /* collision distance */

#define E1000_TXD_CMD_EOP    	(0x01000000>>24) /* End of Packet */
#define E1000_TXD_CMD_RS 	(0x08000000>>24) /* Report Status */
#define E1000_TXD_STAT_DD 	0x00000001 /* Descriptor Done */

// use 'volatile' to avoid cache and reorder access to this memory
volatile uint32_t *netipc;

#define MAX_TX_NUM 32
#define MAX_BUF_SIZE 1520

/*
 * Since TDLEN must be 128-byte aligned and each transmit descriptor is 16 bytes, 
 * your transmit descriptor array will need some multiple of 8 transmit descriptors. 
 * However, don't use more than 64 descriptors or our tests won't be able to test transmit ring overflow.
 */ 
struct tx_desc tx_ring[MAX_TX_NUM];

struct pkt_buf {
	// The maximum size of an Ethernet packet is 1518 bytes
	uint8_t pad[MAX_BUF_SIZE / sizeof(uint8_t)];
} __attribute__((packed));

struct pkt_buf pkt_bufs[MAX_TX_NUM];

int init_e1000(struct pci_func *pcif)
{
	int i;

	// 1.Enable E1000 device
	pci_func_enable(pcif);
	cprintf("reg_base=%016llx, reg_size=%016llx\n", pcif->reg_base[0], pcif->reg_size[0]);
	
	// 2.Build memory map for BAR-0
	netipc = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);
	cprintf("FD Status: %08x\n", netipc[E1000_STATUS]);

	// 3.Initialize Transmit Ring Buffer and relevant registers
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

	// 4.Initialize Transmit Control Register
	netipc[E1000_TCTL] |= E1000_TCTL_EN; 
	netipc[E1000_TCTL] |= E1000_TCTL_PSP; 
	netipc[E1000_TCTL] |= (0x10<<4); 
	netipc[E1000_TCTL] |= (0x40<<12); 
	cprintf("TCTL: %016llx\n", netipc[E1000_TCTL]);

	// NOTE: According to the IEEE802.3 standard, IPGR1 should be 2/3 of IPGR2 value.
	netipc[E1000_TIPG] = (0x6<<20) | (0x4<<10) | (0xa<<0);
	cprintf("TIPG: %016llx\n", netipc[E1000_TIPG]);

	// Test send packet
	send_packet("hello world", 64);
	send_packet("world hello", 128);

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
	netipc[E1000_TDT] += 1;

	tx->cmd |= (E1000_TXD_CMD_RS | E1000_TXD_CMD_EOP);

	return 0;
}

