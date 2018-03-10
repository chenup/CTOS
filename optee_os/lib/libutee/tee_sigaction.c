#include <tee_signal.h>

int sigaction(int sig, const struct sigaction *act, struct sigaction *oact)
{
	message m;
  	memset(&m, 0, sizeof(m));
  	m.m_pm_sig.nr = sig;
  	m.m_pm_sig.act = (vir_bytes)act;
  	m.m_pm_sig.oact = (vir_bytes)oact;
  	//m.m_pm_sig.ret = (vir_bytes)__sigreturn;

  	return _syscall(0, M_TYPE_SIGACTION, &m);
}