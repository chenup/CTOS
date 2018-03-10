#include <tee_pm_proto.h>
#include <tee_errno.h>

int do_sigaction(void)
{
  int res;
  int sig_nr;
  struct sigaction svec;
  struct sigaction *svp;

  sig_nr = m_in.m_pm_sig.nr;
  if (sig_nr == SIGKILL)
  {
    return TEE_OK;
  }
  if (sig_nr < 1 || sig_nr >= NSIG) 
  {
    return TEE_EINVAL;
  }

  svp = &mp->mp_sigact[sig_nr];
  if (m_in.m_pm_sig.oact != 0) 
  {
    res = sys_datacopy(PM_PROC_NR,(vir_bytes) svp, who_e, m_in.m_pm_sig.oact, (phys_bytes) sizeof(svec));
    if (res != TEE_OK) 
    {
      return res;
    }
  }

  if (m_in.m_pm_sig.act == 0)
  {
  	return TEE_OK;
  }

  /* Read in the sigaction structure. */
  res = sys_datacopy(who_e, m_in.m_pm_sig.act, PM_PROC_NR, (vir_bytes) &svec,
	  (phys_bytes) sizeof(svec));
  if (res != TEE_OK)
  {
    return res;
  }

  if (svec.sa_handler == SIG_IGN) 
  {
  	sigaddset(&mp->mp_ignore, sig_nr);
  	sigdelset(&mp->mp_sigpending, sig_nr);
  	sigdelset(&mp->mp_ksigpending, sig_nr);
  	sigdelset(&mp->mp_catch, sig_nr);
  } 
  else if (svec.sa_handler == SIG_DFL) 
  {
  	sigdelset(&mp->mp_ignore, sig_nr);
  	sigdelset(&mp->mp_catch, sig_nr);
  } 
  else 
  {
  	sigdelset(&mp->mp_ignore, sig_nr);
  	sigaddset(&mp->mp_catch, sig_nr);
  }
  mp->mp_sigact[sig_nr].sa_handler = svec.sa_handler;
  sigdelset(&svec.sa_mask, SIGKILL);
  sigdelset(&svec.sa_mask, SIGSTOP);
  mp->mp_sigact[sig_nr].sa_mask = svec.sa_mask;
  mp->mp_sigact[sig_nr].sa_flags = svec.sa_flags;
  mp->mp_sigreturn = m_in.m_pm_sig.ret;
  return TEE_OK;
}

int do_kill(void)
{
/* Perform the kill(pid, signo) system call. */

  return check_sig(m_in.m_pm_sig.pid, m_in.m_pm_sig.nr, FALSE /* ksig */);
}

#define INIT_PID     1  /* INIT's process id number */

int check_sig(proc_id, signo, ksig)
uint32_t proc_id;      /* pid of proc to sig, or 0 or -1, or -pgrp */
int signo;      /* signal to send to process (0 to _NSIG-1) */
int ksig;     /* non-zero means signal comes from kernel  */
{
/* Check to see if it is possible to send a signal.  The signal may have to be
 * sent to a group of processes.  This routine is invoked by the KILL system
 * call, and also when the kernel catches a DEL or other signal.
 */

  register struct mproc *rmp;
  int count;      /* count # of signals sent */
  int error_code;

  if (signo < 0 || signo >= NSIG) 
  {
    return TEE_EINVAL;
  }

  /* Return EINVAL for attempts to send SIGKILL to INIT alone. */
  if (proc_id == INIT_PID && signo == SIGKILL)
  {
    return TEE_EINVAL;
  }

  /* Signal RS first when broadcasting SIGTERM. */
  if (proc_id == -1 && signo == SIGTERM)
  {
    sys_kill(RS_PROC_NR, signo);
  }

  /* Search the proc table for processes to signal. Start from the end of the
   * table to analyze core system processes at the end when broadcasting.
   * (See forkexit.c about pid magic.)
   */
  count = 0;
  error_code = ESRCH;
  for (rmp = &mproc[NR_PROCS-1]; rmp >= &mproc[0]; rmp--) {
  if (!(rmp->mp_flags & IN_USE)) continue;

  /* Check for selection. */
  if (proc_id > 0 && proc_id != rmp->mp_pid) continue;
  if (proc_id == 0 && mp->mp_procgrp != rmp->mp_procgrp) continue;
  if (proc_id == -1 && rmp->mp_pid <= INIT_PID) continue;
  if (proc_id < -1 && rmp->mp_procgrp != -proc_id) continue;

  /* Do not kill servers and drivers when broadcasting SIGKILL. */
  if (proc_id == -1 && signo == SIGKILL &&
    (rmp->mp_flags & PRIV_PROC)) continue;

  /* Skip VM entirely as it might lead to a deadlock with its signal
   * manager if the manager page faults at the same time.
   */
  if (rmp->mp_endpoint == VM_PROC_NR) continue;

  /* Disallow lethal signals sent by user processes to sys processes. */
  if (!ksig && SIGS_IS_LETHAL(signo) && (rmp->mp_flags & PRIV_PROC)) {
      error_code = EPERM;
      continue;
  }

  /* Check for permission. */
  if (mp->mp_effuid != SUPER_USER
      && mp->mp_realuid != rmp->mp_realuid
      && mp->mp_effuid != rmp->mp_realuid
      && mp->mp_realuid != rmp->mp_effuid
      && mp->mp_effuid != rmp->mp_effuid) {
    error_code = EPERM;
    continue;
  }

  count++;
  if (signo == 0 || (rmp->mp_flags & EXITING)) continue;

  /* 'sig_proc' will handle the disposition of the signal.  The
   * signal may be caught, blocked, ignored, or cause process
   * termination, possibly with core dump.
   */
  sig_proc(rmp, signo, TRUE /*trace*/, ksig);

  if (proc_id > 0) break; /* only one process being signaled */
  }

  /* If the calling process has killed itself, don't reply. */
  if ((mp->mp_flags & (IN_USE | EXITING)) != IN_USE) return(SUSPEND);
  return(count > 0 ? OK : error_code);
}