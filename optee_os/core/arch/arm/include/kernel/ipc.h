/*
	2018-2-2
*/
#ifndef KERNEL_IPC_H
#define KERNEL_IPC_H

//TODO 2018-2-6
// p_rts_flags
#define P_SENDING (1 << 0) 
#define P_RECVING (1 << 1)

struct message {
	int from;
	int to;
	char msg[64];
};

#endif