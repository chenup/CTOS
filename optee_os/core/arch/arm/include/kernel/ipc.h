/*
	2018-2-2
*/
#ifndef KERNEL_IPC_H
#define KERNEL_IPC_H

//TODO 2018-2-6
// p_rts_flags
#define P_SENDING (1 << 0) 
#define P_RECVING (1 << 1)

//TODO 2018-2-12
#define M_TYPE_NOTIFY 1
//TODO 2018-2-12
#define M_TYPE_FORK 2

struct message {
    int from;
    int to;
    int type;
    union {
        char msg[64];
        uint64_t ts;
		int mp_pid;
    } u;
};

#endif