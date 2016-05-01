#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

#include <kern/pci.h>
#include <inc/env.h>

#define E1000_STATUS 	(0x00008/4)  /* Device Status - RO */
#define E1000_EECD     	(0x00010/4)  /* EEPROM/Flash Control - RW */
#define E1000_TCTL 	(0x00400/4)  /* TX Control - RW */
#define E1000_TIPG 	(0x00410/4)  /* TX Inter-packet gap -RW */
#define E1000_TDBAL 	(0x03800/4)  /* TX Descriptor Base Address Low - RW */
#define E1000_TDBAH 	(0x03804/4)  /* TX Descriptor Base Address High - RW */
#define E1000_TDLEN 	(0x03808/4)  /* TX Descriptor Length - RW */
#define E1000_TDH 	(0x03810/4)  /* TX Descriptor Head - RW */
#define E1000_TDT 	(0x03818/4)  /* TX Descripotr Tail - RW */

#define E1000_STATUS_FD	0x00000001    /* Full duplex.0=half,1=full */

#define E1000_EECD_REQ 	0x00000040 /* EEPROM Access Request */
#define E1000_EECD_GNT 	0x00000080 /* EEPROM Access Grant */

#define E1000_TCTL_EN 	0x00000002    /* enable tx */
#define E1000_TCTL_PSP 	0x00000008    /* pad short packets */
#define E1000_TCTL_CT 	0x00000ff0    /* collision threshold */
#define E1000_TCTL_COLD 0x003ff000    /* collision distance */

#define E1000_TXD_CMD_EOP    	(0x01000000>>24) /* End of Packet */
#define E1000_TXD_CMD_RS 	(0x08000000>>24) /* Report Status */
#define E1000_TXD_STAT_DD 	0x00000001 /* Descriptor Done */

/* Receiving Packets */

#define E1000_IMS      0x000D0  /* Interrupt Mask Set - RW */

#define E1000_RCTL	(0x00100/4)		/* Receive Control Register - RW */
/* Receive Control */
#define E1000_RCTL_EN		0x00000002	/* enable */
#define E1000_RCTL_MPE		0x00000010	/* multicast promiscuous enab */
#define E1000_RCTL_LPE		0x00000020	/* long packet enable */
#define E1000_RCTL_BAM		0x00008000	/* broadcast enable */
/* these buffer sizes are valid if E1000_RCTL_BSEX is 0 */
#define E1000_RCTL_SZ_2048	0x00000000	/* rx buffer size 2048 */
#define E1000_RCTL_SZ_1024	0x00010000	/* rx buffer size 1024 */
#define E1000_RCTL_SZ_512	0x00020000	/* rx buffer size 512 */
#define E1000_RCTL_SZ_256	0x00030000	/* rx buffer size 256 */
/* these buffer sizes are valid if E1000_RCTL_BSEX is 1 */
#define E1000_RCTL_SZ_16384  	0x00010000	/* rx buffer size 16384 */
#define E1000_RCTL_SZ_8192	0x00020000	/* rx buffer size 8192 */
#define E1000_RCTL_SZ_4096	0x00030000	/* rx buffer size 4096 */
#define E1000_RCTL_BSEX		0x02000000	/* Buffer size extension */
#define E1000_RCTL_SECRC	0x04000000	/* Strip Ethernet CRC */

#define E1000_RAL(n)	((0x05400 + 8*n)/4)	/* Receive Address Low, 16 reisters contain
						   the lower bits of the 48-bit Ethernet address - RW */
#define E1000_RAH(n)	((0x05404 + 8*n)/4)	/* Receive Address High, 16 registers contain - RW
						   the upper bits of the 48-bit Ethernet address */
// Note: When writing to this register, always write low-to-high.
//       When clearing this register, always clear high-to-low
/* Receive Address */
#define E1000_RAH_AV		0x80000000	/* Receive descriptor valid */

#define E1000_RDBAL	(0x02800/4)		/* Receive Descriptor Base Address Low - RW */
#define E1000_RDBAH	(0x02804/4)		/* Receive Descriptor Base Address High - RW */
#define E1000_RDLEN	(0x02808/4)		/* Receive Descriptor Length - RW
						   Bit 19:7. in a multiple of eight */
#define E1000_RDH	(0x02810/4)		/* Receive Descriptor Head - RW */
#define	E1000_RDT	(0x02818/4)		/* Receive Descriptor Tail - RW */

#define E1000_MTA	(0x05200/4)		  /* Multicast Table Array - RW Array */


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

/* Receive Descriptor */
struct e1000_rx_desc {
    uint64_t buffer_addr; 	/* Address of the descriptor's data buffer */
    uint16_t length;    	 /* Length of data DMAed into data buffer */
    uint16_t csum;		/* Packet checksum */
    uint8_t status;		/* Descriptor status */
    uint8_t errors;		/* Descriptor Errors */
    uint16_t special;
};

/* Receive Descriptor bit definitions */
#define E1000_RXD_STAT_DD       0x01    /* Descriptor Done */

#define N_RX_NUM (128)		// make it greater than 100 to pass the grade.
				// TODO: fix the input 100 test failure issue.

int init_e1000(struct pci_func *pcif);
int send_packet(void *packet, size_t len);
int send_packet_direct(void *packet, size_t len);
int recv_packet(void *packet, size_t *len);

#endif	// JOS_KERN_E1000_H

