/*	$OpenBSD: uthread_fork.c,v 1.19 2008/04/04 19:30:41 kurt Exp $	*/
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
 * $FreeBSD: uthread_fork.c,v 1.14 1999/09/29 15:18:38 marcel Exp $
 */
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

pid_t _dofork(int vfork);

pid_t
fork(void)
{
	pid_t pid;
	struct pthread_atfork *af;

	/*
	 * Defer signals to protect the scheduling queues from access
	 * by the signal handler:
	 */
	_thread_kern_sig_defer();

	pthread_mutex_lock(&_atfork_mutex);

	/* Run down atfork prepare handlers. */
	TAILQ_FOREACH_REVERSE(af, &_atfork_list, atfork_head, qe) {
		if (af->prepare != NULL)
			af->prepare();
	}

	if ((pid = _dofork(0)) == 0) {
		/* Run down atfork child handlers. */
		TAILQ_FOREACH(af, &_atfork_list, qe) {
			if (af->child != NULL)
				af->child();
		}
		_mutex_reinit(&_atfork_mutex);
	} else {
		/* Run down atfork parent handlers. */
		TAILQ_FOREACH(af, &_atfork_list, qe) {
			if (af->parent != NULL)
				af->parent();
		}
		pthread_mutex_unlock(&_atfork_mutex);
	}

	/*
	 * Undefer and handle pending signals, yielding if necessary:
	 */
	_thread_kern_sig_undefer();

	return(pid);
}

pid_t
_dofork(int vfork)
{
	struct pthread	*curthread = _get_curthread();
	int             i, flags;
	pid_t           ret;
	pthread_t	pthread;

	/* Fork a new process and reset child's state */
	if ((ret = (vfork ? _thread_sys_vfork() : _thread_sys_fork())) == 0) {
		/* Close the pthread kernel pipe: */
		_thread_sys_close(_thread_kern_pipe[0]);
		_thread_sys_close(_thread_kern_pipe[1]);

		/* Reset signals pending for the running thread: */
		sigemptyset(&curthread->sigpend);

		/*
		 * Create a pipe that is written to by the signal handler to
		 * prevent signals being missed in calls to
		 * _thread_sys_select: 
		 */
		if (_thread_sys_pipe(_thread_kern_pipe) != 0) {
			/* Cannot create pipe, so abort: */
			PANIC("Cannot create pthread kernel pipe for forked process");
		}
		/* Get the flags for the read pipe: */
		else if ((flags = _thread_sys_fcntl(_thread_kern_pipe[0], F_GETFL, NULL)) == -1) {
			/* Abort this application: */
			abort();
		}
		/* Make the read pipe non-blocking: */
		else if (_thread_sys_fcntl(_thread_kern_pipe[0], F_SETFL, flags | O_NONBLOCK) == -1) {
			/* Abort this application: */
			abort();
		}
		/* Get the flags for the write pipe: */
		else if ((flags = _thread_sys_fcntl(_thread_kern_pipe[1], F_GETFL, NULL)) == -1) {
			/* Abort this application: */
			abort();
		}
		/* Make the write pipe non-blocking: */
		else if (_thread_sys_fcntl(_thread_kern_pipe[1], F_SETFL, flags | O_NONBLOCK) == -1) {
			/* Abort this application: */
			abort();
		}
		/* Reinitialize the GC mutex: */
		else if (_mutex_reinit(&_gc_mutex) != 0) {
			/* Abort this application: */
			PANIC("Cannot initialize GC mutex for forked process");
		}
		/* Reinitialize the GC condition variable: */
		else if (_cond_reinit(&_gc_cond) != 0) {
			/* Abort this application: */
			PANIC("Cannot initialize GC condvar for forked process");
		}
		/* Initialize the ready queue: */
		else if (_pq_init(&_readyq) != 0) {
			/* Abort this application: */
			PANIC("Cannot initialize priority ready queue.");
		} else {
			/*
			 * Enter a loop to remove all threads other than
			 * the running thread from the thread list:
			 */
			while ((pthread = TAILQ_FIRST(&_thread_list)) != NULL) {
				TAILQ_REMOVE(&_thread_list, pthread, tle);

				/* Make sure this isn't the running thread: */
				if (pthread != curthread) {
					/* XXX should let gc do all this. */
					if(pthread->stack != NULL)
						_thread_stack_free(pthread->stack);

					if (pthread->specific_data != NULL)
						free(pthread->specific_data);

					if (pthread->poll_data.fds != NULL)
						free(pthread->poll_data.fds);

					free(pthread);
				}
			}

			/* Restore the running thread */
			TAILQ_INSERT_HEAD(&_thread_list, curthread, tle);

			/* Re-init the dead thread list: */
			TAILQ_INIT(&_dead_list);

			/* Re-init the waiting and work queues. */
			TAILQ_INIT(&_waitingq);
			TAILQ_INIT(&_workq);

			/* Re-init the threads mutex queue: */
			TAILQ_INIT(&curthread->mutexq);

			/* No spinlocks yet: */
			_spinblock_count = 0;

			/* Don't queue signals yet: */
			_queue_signals = 0;

			/* Initialize signal handling: */
			_thread_sig_init();

			/* Initialize the scheduling switch hook routine: */
			_sched_switch_hook = NULL;

			/* Clear out any locks in the file descriptor table: */
			for (i = 0; i < _thread_max_fdtsize; i++) {
				if (_thread_fd_table[i] != NULL) {
					/* Initialise the file locks: */
					_SPINLOCK_INIT(&_thread_fd_table[i]->lock);
					_thread_fd_table[i]->r_owner = NULL;
					_thread_fd_table[i]->w_owner = NULL;
					_thread_fd_table[i]->r_fname = NULL;
					_thread_fd_table[i]->w_fname = NULL;
					_thread_fd_table[i]->r_lineno = 0;
					_thread_fd_table[i]->w_lineno = 0;
					_thread_fd_table[i]->r_lockcount = 0;
					_thread_fd_table[i]->w_lockcount = 0;
					if (_thread_fd_table[i]->status_flags != NULL)
						_SPINLOCK_INIT(&_thread_fd_table[i]->status_flags->lock);
					/*
					 * If a fd was in the process of closing
					 * then finish closing it.
					 */
					if (_thread_fd_table[i]->state == FD_ENTRY_CLOSING) {
						 _thread_fd_entry_close(i);
						 _thread_sys_close(i);
					}

					/* Initialise the read/write queues: */
					TAILQ_INIT(&_thread_fd_table[i]->r_queue);
					TAILQ_INIT(&_thread_fd_table[i]->w_queue);
				}
			}
		}
	}

	/* Return the process ID: */
	return (ret);
}
#endif
