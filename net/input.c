#include "ns.h"

extern union Nsipc nsipcbuf;

void
input(envid_t ns_envid)
{
	binaryname = "ns_input";

	// LAB 6: Your code here:
	// 	- read a packet from the device driver
	//	- send it to the network server
	// Hint: When you IPC a page to the network server, it will be
	// reading from it for a while, so don't immediately receive
	// another packet in to the same physical page.
	while (1)
	{
		int r;
		size_t len = 1024;
		char pkt[1024];

		int perm = PTE_U | PTE_P | PTE_W;
		while ((r = sys_recv_packet(pkt, &len)) < 0)
			sys_yield();

		if ((r = sys_page_alloc(0, &nsipcbuf, perm)) < 0)
			panic("sys_page_alloc error in input: %d\n", r);

		nsipcbuf.pkt.jp_len = len;
		memmove(nsipcbuf.pkt.jp_data, pkt, len);
		ipc_send(ns_envid, NSREQ_INPUT, &nsipcbuf, perm);
	}
}
