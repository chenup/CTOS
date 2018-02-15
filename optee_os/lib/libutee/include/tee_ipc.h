#ifndef TEE_IPC_H
#define TEE_IPC_H

#define M_TYPE_NOTIFY 1
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