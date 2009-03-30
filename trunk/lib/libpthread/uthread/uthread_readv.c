/*	$OpenBSD: uthread_readv.c,v 1.9 2006/10/03 02:59:36 kurt Exp $	*/
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
 * $FreeBSD: uthread_readv.c,v 1.8 1999/08/28 00:03:43 peter Exp $
 *
 */
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/uio.h>
#include <errno.h>
#include <unistd.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

ssize_t
readv(int fd, const struct iovec * iov, int iovcnt)
{
	struct pthread	*curthread = _get_curthread();
	ssize_t	ret;
	int	type;

	/* This is a cancellation point: */
	_thread_enter_cancellation_point();

	/* Lock the file descriptor for read: */
	if ((ret = _FD_LOCK(fd, FD_READ, NULL)) == 0) {
		/* Get the read/write mode type: */
		type = _thread_fd_table[fd]->status_flags->flags & O_ACCMODE;

		/* Check if the file is not open for read: */
		if (type != O_RDONLY && type != O_RDWR) {
			/* File is not open for read: */
			errno = EBADF;
			_FD_UNLOCK(fd, FD_READ);
			_thread_leave_cancellation_point();
			return (-1);
		}

		/* Perform a non-blocking readv syscall: */
		while ((ret = _thread_sys_readv(fd, iov, iovcnt)) < 0) {
			if ((_thread_fd_table[fd]->status_flags->flags & O_NONBLOCK) == 0 &&
			    (errno == EWOULDBLOCK || errno == EAGAIN)) {
				curthread->data.fd.fd = fd;
				_thread_kern_set_timeout(NULL);

				/* Reset the interrupted operation flag: */
				curthread->interrupted = 0;
				curthread->closing_fd = 0;

				_thread_kern_sched_state(PS_FDR_WAIT,
				    __FILE__, __LINE__);

				/*
				 * Check if the operation was
				 * interrupted by a signal or
				 * a closing fd.
				 */
				if (curthread->interrupted) {
					errno = EINTR;
					ret = -1;
					break;
				} else if (curthread->closing_fd) {
					errno = EBADF;
					ret = -1;
					break;
				}
			} else {
				break;
			}
		}
		_FD_UNLOCK(fd, FD_READ);
	}

	/* No longer in a cancellation point: */
	_thread_leave_cancellation_point();

	return (ret);
}
#endif
