#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

#include <kern/pci.h>

struct tx_desc
{
	uint64_t addr;
	uint16_t length;
	uint8_t cso;
	uint8_t cmd;
	uint8_t status;
	uint8_t css;
	uint16_t special;
};

int init_e1000(struct pci_func *pcif);

int send_packet(void *packet, size_t len);

#endif	// JOS_KERN_E1000_H

