#ifndef TEE_IPC_H
#define TEE_IPC_H

#define M_TYPE_NOTIFY 1
#define M_TYPE_FORK 2
//TODO 2018-3-8
#define M_TYPE_SIGACTION 3
//TODO 2018-3-8
#define M_TYPE_KILL 4

//TODO 2018-3-8
typedef struct {
	//pid_t pid;
    uint32_t pid;
	int nr;
	vir_bytes act;		/* const struct sigaction * */
	vir_bytes oact;		/* struct sigaction * */
	vir_bytes ret;		/* int (*)(void) */

	//uint8_t padding[36];
} mess_pm_sig;

struct message {
    int from;
    int to;
    int type;
    //TODO 2018-3-8
    mess_pm_sig m_pm_sig;
    union {
        char msg[64];
        uint64_t ts;
		int mp_pid;
    } u;
};


#endif