/*	$OpenBSD: uthread_wait4.c,v 1.7 2001/11/09 00:20:26 marc Exp $	*/
/*
 * Copyright (c) 1995-1998 John Birrell <jb@cimlogic.com.au>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by John Birrell.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JOHN BIRRELL AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: uthread_wait4.c,v 1.5 1999/08/28 00:03:53 peter Exp $
 */
#include <errno.h>
#include <sys/wait.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

/*
 * Note: a thread calling wait4 may have its state changed to waiting
 * until awakened by a signal.  Also note that system(3), for example,
 * blocks SIGCHLD and calls waitpid (which calls wait4).  If the process
 * started by system(3) doesn't finish before this function is called the
 * function will never awaken -- system(3) also ignores SIGINT and SIGQUIT.
 *
 * Thus always unmask SIGCHLD here.
 */
pid_t
wait4(pid_t pid, int *istat, int options, struct rusage * rusage)
{
	struct pthread	*curthread = _get_curthread();
	pid_t           ret;
	sigset_t	mask, omask;

	/* This is a cancellation point: */
	_thread_enter_cancellation_point();

	_thread_kern_sig_defer();

	sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);
	sigprocmask(SIG_UNBLOCK, &mask, &omask);

	/* Perform a non-blocking wait4 syscall: */
	while ((ret = _thread_sys_wait4(pid, istat, options | WNOHANG, rusage)) == 0 && (options & WNOHANG) == 0) {
		/* Reset the interrupted operation flag: */
		curthread->interrupted = 0;

		/* Schedule the next thread while this one waits: */
		_thread_kern_sched_state(PS_WAIT_WAIT, __FILE__, __LINE__);

		/* Check if this call was interrupted by a signal: */
		if (curthread->interrupted) {
			errno = EINTR;
			ret = -1;
			break;
		}
	}

	sigprocmask(SIG_SETMASK, &omask, NULL);

	_thread_kern_sig_undefer();

	/* No longer in a cancellation point: */
	_thread_leave_cancellation_point();

	return (ret);
}
#endif
