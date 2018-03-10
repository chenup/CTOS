#include <tee_signal.h>

int kill(uint32_t pid, int sig)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.m_pm_sig.pid = pid;
  m.m_pm_sig.nr = sig;
  return _syscall(0, M_TYPE_KILL, &m);
}
