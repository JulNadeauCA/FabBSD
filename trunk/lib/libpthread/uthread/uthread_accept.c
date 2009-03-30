/*	$OpenBSD: uthread_accept.c,v 1.11 2006/10/03 02:59:36 kurt Exp $	*/
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
 * $FreeBSD: uthread_accept.c,v 1.9 1999/08/28 00:03:20 peter Exp $
 */
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

int
accept(int fd, struct sockaddr * name, socklen_t *namelen)
{
	struct pthread	*curthread = _get_curthread();
	int             ret;
	int		newfd;
	enum fd_entry_mode init_mode;

	/* This is a cancellation point: */
	_thread_enter_cancellation_point();

	/* Lock the file descriptor: */
	if ((ret = _FD_LOCK(fd, FD_RDWR, NULL)) == 0) {
		/* Enter a loop to wait for a connection request: */
		while ((ret = _thread_sys_accept(fd, name, namelen)) < 0) {
			/* Check if the socket is to block: */
			if ((_thread_fd_table[fd]->status_flags->flags & O_NONBLOCK) == 0 &&
			    (errno == EWOULDBLOCK || errno == EAGAIN)) {
				/* Save the socket file descriptor: */
				curthread->data.fd.fd = fd;
				curthread->data.fd.fname = __FILE__;
				curthread->data.fd.branch = __LINE__;

				/* Set the timeout: */
				_thread_kern_set_timeout(NULL);
				curthread->interrupted = 0;
				curthread->closing_fd = 0;

				/* Schedule the next thread: */
				_thread_kern_sched_state(PS_FDR_WAIT, __FILE__,
							 __LINE__);

				/* Check if the wait was interrupted: */
				if (curthread->interrupted) {
					/* Return an error status: */
					errno = EINTR;
					ret = -1;
					break;
				} else if (curthread->closing_fd) {
					/* Return an error status: */
					errno = EBADF;
					ret = -1;
					break;
				}
			} else {
				/*
				 * Another error has occurred, so exit the
				 * loop here: 
				 */
				break;
			}
		}

		/*
		 * If no errors initialize the file descriptor table
		 * for the new socket. If the client's view of the
		 * status_flags for fd is blocking, then force newfd
		 * to be viewed as blocking too.
		 */
		if (ret != -1) {
			newfd = ret;

			if ((_thread_fd_table[fd]->status_flags->flags & O_NONBLOCK) == 0)
				init_mode = FD_INIT_BLOCKING;
			else
				init_mode = FD_INIT_NEW;
			if((ret = _thread_fd_table_init(newfd, init_mode, NULL)) != -1)
				ret = newfd;
			else {
				/* quitely close the fd */
				_thread_sys_close(ret);
			}
		}

		/* Unlock the file descriptor: */
		_FD_UNLOCK(fd, FD_RDWR);
	}

	/* No longer in a cancellation point: */
	_thread_leave_cancellation_point();

	/* Return the socket file descriptor or -1 on error: */
	return (ret);
}
#endif
