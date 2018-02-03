/*
	2018-2-2
*/
#ifndef KERNEL_IPC_H
#define KERNEL_IPC_H

struct message {
	int from;
	int to;
	char msg[64];
};

#endif