#include "ns.h"

extern union Nsipc nsipcbuf;

void
output(envid_t ns_envid)
{
	binaryname = "ns_output";

	// LAB 6: Your code here:
	// 	- read a packet from the network server
	//	- send the packet to the device driver
	
	int req;
	envid_t srcenv;
	struct jif_pkt *pkt;

	while (1) {
		req = ipc_recv(&srcenv, &nsipcbuf, NULL);

		pkt = &(nsipcbuf.pkt);

		cprintf("Receive package to transmit, len: %d\n", pkt->jp_len);

		//sys_send_packet(pkt->jp_data, pkt->jp_len);
		sys_send_packet_direct(pkt->jp_data, pkt->jp_len);
	}

}
