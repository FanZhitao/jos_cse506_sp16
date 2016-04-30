#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

#include <kern/pci.h>

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

#define MAX_TX_NUM 32
#define MAX_BUF_SIZE 1518

struct tx_desc
{
	uint64_t addr;
	uint16_t length;
	uint8_t cso;
	uint8_t cmd;
	uint8_t status;
	uint8_t css;
	uint16_t special;
} __attribute__((packed));

struct pkt_buf {
	// The maximum size of an Ethernet packet is 1518 bytes
	uint8_t pad[MAX_BUF_SIZE / sizeof(uint8_t)];
} __attribute__((packed));


int init_e1000(struct pci_func *pcif);
int send_packet(void *packet, size_t len);

#endif	// JOS_KERN_E1000_H

