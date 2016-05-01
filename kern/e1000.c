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

struct e1000_rx_desc rx_ring[N_RX_NUM];

struct pkt_buf rx_pkt_bufs[N_RX_NUM];


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
	int i;
	uintptr_t pa;
	memset(rx_ring, 0, sizeof(struct e1000_rx_desc) * N_RX_NUM);
	memset(rx_pkt_bufs, 0, sizeof(struct pkt_buf) * N_RX_NUM);
	for (i = 0; i < N_RX_NUM; i++)
	{
		pa = (uintptr_t)PADDR(rx_pkt_bufs+i);
		rx_ring[i].buffer_addr = (uintptr_t)PADDR(rx_pkt_bufs + i);
	}

	// Program the Receive Address Register(s) (RAL/RAH) with the desired Ethernet addresses.
	// hardcoded mac address: 52:54:00:12:34:56
	// TODO: challenge - read form EEPROM
	netipc[E1000_RAL(0)] = 0x12005452; // low-order 32 bits
	netipc[E1000_RAH(0)] = 0x5634 | E1000_RAH_AV; // high-order 16 bits with "Address Valid" bit 

	// Initialize the MTA (Multicast Table Array) to 0b.
	netipc[E1000_MTA] = 0;

	// Program the Interrupt Mask Set/Read (IMS) register to enable any interrupt
	// the software driver wants to be notified of when the event occurs.
	// Disable it here.
	netipc[E1000_IMS] = 0;

	// Allocate a region of memory for the receive descriptor list.
	// Software should insure this memory is aligned on a paragraph (16-byte) boundary.
	// Program the Receive Descriptor Base Address (RDBAL/RDBAH) register(s) with the address
	// of the region.
	pa = (uintptr_t)PADDR(rx_ring);
	netipc[E1000_RDBAL] = pa;
	netipc[E1000_RDBAH] = pa >> 32;

	// Set the Receive Descriptor Length (RDLEN) register to the size (in bytes) of the descriptor ring.
	// This register must be 128-byte aligned.
	netipc[E1000_RDLEN] = sizeof(struct e1000_rx_desc) * N_RX_NUM;

	// Receive buffers of appropriate size should be allocated and pointers to these buffers
	// should be allocated and pointers to these buffers should be stored in the receive descriptor ring.
	// Head should point to the first valid receive descriptor in the descriptor ring and tail
	// should point to one descriptor beyond the last valid descriptor in the descriptor ring.
	pa = (uintptr_t)PADDR(rx_ring);
	netipc[E1000_RDH] = 0;
	netipc[E1000_RDT] = N_RX_NUM - 1;

	// Program the Receive Control (RCTL) register with appropriate values
	// for desired operation to include the following:
	//
	// Set the receiver Enable (RECT.EN) bit to 1b.
	netipc[E1000_RCTL] |= E1000_RCTL_EN;
	// Configure the Receive Buffer Size (RCTL.BSIZE) bits to reflect the size
	// of the receive buffers software provides to hardware.
	netipc[E1000_RCTL] |= E1000_RCTL_SZ_1024;
	// Set the Broadcast Accept Mode (RCTL.BAM) bit to 1b allowing the hardware
	// to accept broadcast packets.
	netipc[E1000_RCTL] |= E1000_RCTL_BAM;
	// Set the Strip Ethernet CRC (RCTL.SECRC) to strip the CRC.
	netipc[E1000_RCTL] |= E1000_RCTL_SECRC; // set strip Etherenet CRC on
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

int recv_packet(void *packet, size_t *len)
{
	int tail = netipc[E1000_RDT];
	struct e1000_rx_desc *rx = rx_ring + (tail + 1) % N_RX_NUM;
	// If DD status bit is set, a packet has been delivered to this descriptor's packet buffer
	// And make sure RDH never equals to RDT.
	if ((rx->status & E1000_RXD_STAT_DD) && ((tail + 1) % N_RX_NUM != netipc[E1000_RDH]))
	{
		*len = rx->length;

		// copy the packet data out of buffer
		memcpy(packet, KADDR(rx->buffer_addr), *len);

		rx->status &= ~E1000_RXD_STAT_DD;

		// tell the card the descriptor is free by updating the queue's tail index, RDT
		netipc[E1000_RDT] = (tail + 1) % N_RX_NUM;

		return 0;
	}

	return -1; // try again
}
