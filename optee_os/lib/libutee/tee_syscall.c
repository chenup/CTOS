#include <tee_ipc.h>

int _syscall(endpoint_t who, int syscallnr, message *msgptr)
{
	int res;
	msgptr->type = syscallnr;
	res = utee_sendrec(0, msgptr);
	if(res == 0)
	{
		return msg.u.mp_pid;
	}
	else
	{
		return -1;
	}
}