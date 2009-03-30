/*	$OpenBSD: uthread_create.c,v 1.22 2008/07/07 04:59:22 guenther Exp $	*/
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
 * $FreeBSD: uthread_create.c,v 1.19 1999/08/28 00:03:28 peter Exp $
 */
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stddef.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/mman.h>
#ifdef _THREAD_SAFE
#include <machine/reg.h>
#include <pthread.h>
#include "pthread_private.h"

/* Symbols that help gdb find the information it needs. */
int _thread_next_offset = offsetof(struct pthread, tle.tqe_next);
int _thread_state_offset = offsetof(struct pthread, state);

int
pthread_create(pthread_t *thread, const pthread_attr_t *attr,
	       void *(*start_routine) (void *), void *arg)
{
	struct pthread	*curthread = _get_curthread();
	struct itimerval itimer;
	int		f_gc = 0;
	int             ret = 0;
	pthread_t       gc_thread;
	pthread_t       new_thread;
	pthread_attr_t	pattr;
	struct stack   *stack;

	if (thread == NULL)
		return(EINVAL);

	/*
	 * Locking functions in libc are required when there are
	 * threads other than the initial thread.
	 */
	__isthreaded = 1;

	/* Allocate memory for the thread structure: */
	if ((new_thread = (pthread_t) malloc(sizeof(struct pthread))) == NULL) {
		/* Insufficient memory to create a thread: */
		ret = EAGAIN;
	} else {
		/* Check if default thread attributes are required: */
		if (attr == NULL || *attr == NULL) {
			/* Use the default thread attributes: */
			pattr = &pthread_attr_default;
		} else {
			pattr = *attr;
		}

		/* Allocate a stack: */
		stack = _thread_stack_alloc(pattr->stackaddr_attr,
		    pattr->stacksize_attr);
		if (stack == NULL) {
			ret = EAGAIN;
			free(new_thread);
		}

		/* Check for errors: */
		if (ret != 0) {
		} else {
			/* Initialise the thread structure: */
			memset(new_thread, 0, sizeof(struct pthread));
			_SPINLOCK_INIT(&new_thread->lock);
			new_thread->slice_usec = -1;
			new_thread->sig_saved = 0;
			new_thread->stack = stack;
			new_thread->start_routine = start_routine;
			new_thread->arg = arg;

			new_thread->cancelflags = PTHREAD_CANCEL_ENABLE |
			    PTHREAD_CANCEL_DEFERRED;

			/*
			 * Write a magic value to the thread structure
			 * to help identify valid ones:
			 */
			new_thread->magic = PTHREAD_MAGIC;

			/* Initialise the thread for signals: */
			new_thread->sigmask = curthread->sigmask;

			/*
			 * Set up new stack frame so that it 'returns' to
			 * the beginning of _thread_start() after it is
			 * switched to:
			 */
			_thread_machdep_init(&new_thread->_machdep,
			    stack->base, stack->size, _thread_start);

			/* Copy the thread attributes: */
			memcpy(&new_thread->attr, pattr, sizeof(struct pthread_attr));

			/*
			 * Check if this thread is to inherit the scheduling
			 * attributes from its parent:
			 */
			if (new_thread->attr.flags & PTHREAD_INHERIT_SCHED) {
				/* Copy the scheduling attributes: */
				new_thread->base_priority =
				    curthread->base_priority &
				    ~PTHREAD_SIGNAL_PRIORITY;
				new_thread->attr.prio =
				    curthread->base_priority &
				    ~PTHREAD_SIGNAL_PRIORITY;
				new_thread->attr.sched_policy =
				    curthread->attr.sched_policy;
			} else {
				/*
				 * Use just the thread priority, leaving the
				 * other scheduling attributes as their
				 * default values:
				 */
				new_thread->base_priority =
				    new_thread->attr.prio;
			}
			new_thread->active_priority = new_thread->base_priority;
			new_thread->inherited_priority = 0;

			/* Initialize joiner to NULL (no joiner): */
			new_thread->joiner = NULL;

			/* Initialize the mutex queue: */
			TAILQ_INIT(&new_thread->mutexq);

			/* Initialise hooks in the thread structure: */
			new_thread->specific_data = NULL;
			new_thread->cleanup = NULL;
			new_thread->flags = 0;
			new_thread->poll_data.nfds = 0;
			new_thread->poll_data.fds = NULL;
			new_thread->continuation = NULL;

			/*
			 * Defer signals to protect the scheduling queues
			 * from access by the signal handler:
			 */
			_thread_kern_sig_defer();

			/*
			 * Check if the garbage collector thread
			 * needs to be started.
			 */
			f_gc = (TAILQ_FIRST(&_thread_list) == _thread_initial);

			/* Add the thread to the linked list of all threads: */
			TAILQ_INSERT_HEAD(&_thread_list, new_thread, tle);

			if (pattr->suspend == PTHREAD_CREATE_SUSPENDED)
				new_thread->state = PS_SUSPENDED;
			else {
				new_thread->state = PS_RUNNING;
				PTHREAD_PRIOQ_INSERT_TAIL(new_thread);
			}

			/*
			 * Undefer and handle pending signals, yielding
			 * if necessary.
			 */
			_thread_kern_sig_undefer();

			/* Return a pointer to the thread structure: */
			if (thread != NULL)
				(*thread) = new_thread;

			if (f_gc != 0) {
				/* Install the scheduling timer: */
				itimer.it_interval.tv_sec = 0;
				itimer.it_interval.tv_usec = _clock_res_usec;
				itimer.it_value = itimer.it_interval;
				if (setitimer(_ITIMER_SCHED_TIMER, &itimer,
				    NULL) != 0)
					PANIC("Cannot set interval timer");
			}

			/* Schedule the new user thread: */
			_thread_kern_sched(NULL);

			/*
			 * Start a garbage collector thread
			 * if necessary.
			 */
			if (f_gc && pthread_create(&gc_thread,NULL,
				    _thread_gc,NULL) != 0)
				PANIC("Can't create gc thread");

		}
	}

	/* Return the status: */
	return (ret);
}

void
_thread_start(void)
{
	struct pthread	*curthread = _get_curthread();

	/* We just left the scheduler via longjmp: */
	_thread_kern_in_sched = 0;

	/* Run the current thread's start routine with argument: */
	pthread_exit(curthread->start_routine(curthread->arg));

	/* This point should never be reached. */
	PANIC("Thread has resumed after exit");
}
#endif
