/*	$OpenBSD: buf.c,v 1.13 2007/05/29 00:19:10 ray Exp $	*/
/*
 * Copyright (c) 2003 Jean-Francois Brousseau <jfb@openbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/queue.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "buf.h"
#include "xmalloc.h"
#include "worklist.h"

#define BUF_INCR	128

struct rcs_buf {
	u_int	cb_flags;

	/* buffer handle, buffer size, and data length */
	u_char	*cb_buf;
	size_t	 cb_size;
	size_t	 cb_len;
};

#define SIZE_LEFT(b)	(b->cb_size - b->cb_len)

static void	rcs_buf_grow(BUF *, size_t);

/*
 * rcs_buf_alloc()
 *
 * Create a new buffer structure and return a pointer to it.  This structure
 * uses dynamically-allocated memory and must be freed with rcs_buf_free(),
 * once the buffer is no longer needed.
 */
BUF *
rcs_buf_alloc(size_t len, u_int flags)
{
	BUF *b;

	b = xmalloc(sizeof(*b));
	/* Postpone creation of zero-sized buffers */
	if (len > 0)
		b->cb_buf = xcalloc(1, len);
	else
		b->cb_buf = NULL;

	b->cb_flags = flags;
	b->cb_size = len;
	b->cb_len = 0;

	return (b);
}

/*
 * rcs_buf_load()
 *
 * Open the file specified by <path> and load all of its contents into a
 * buffer.
 * Returns the loaded buffer on success or NULL on failure.
 * Sets errno on error.
 */
BUF *
rcs_buf_load(const char *path, u_int flags)
{
	int fd;
	ssize_t ret;
	size_t len;
	u_char *bp;
	struct stat st;
	BUF *buf;

	buf = NULL;

	if ((fd = open(path, O_RDONLY, 0600)) == -1)
		goto out;

	if (fstat(fd, &st) == -1)
		goto out;

	if (st.st_size > SIZE_MAX) {
		errno = EFBIG;
		goto out;
	}
	buf = rcs_buf_alloc(st.st_size, flags);
	for (bp = buf->cb_buf; ; bp += (size_t)ret) {
		len = SIZE_LEFT(buf);
		ret = read(fd, bp, len);
		if (ret == -1) {
			int saved_errno;

			saved_errno = errno;
			rcs_buf_free(buf);
			buf = NULL;
			errno = saved_errno;
			goto out;
		} else if (ret == 0)
			break;

		buf->cb_len += (size_t)ret;
	}

out:
	if (fd != -1) {
		int saved_errno;

		/* We may want to preserve errno here. */
		saved_errno = errno;
		(void)close(fd);
		errno = saved_errno;
	}

	return (buf);
}

/*
 * rcs_buf_free()
 *
 * Free the buffer <b> and all associated data.
 */
void
rcs_buf_free(BUF *b)
{
	if (b->cb_buf != NULL)
		xfree(b->cb_buf);
	xfree(b);
}

/*
 * rcs_buf_release()
 *
 * Free the buffer <b>'s structural information but do not free the contents
 * of the buffer.  Instead, they are returned and should be freed later using
 * free().
 */
void *
rcs_buf_release(BUF *b)
{
	void *tmp;

	tmp = b->cb_buf;
	xfree(b);
	return (tmp);
}

/*
 * rcs_buf_get()
 */
u_char *
rcs_buf_get(BUF *b)
{
	return (b->cb_buf);
}

/*
 * rcs_buf_empty()
 *
 * Empty the contents of the buffer <b> and reset pointers.
 */
void
rcs_buf_empty(BUF *b)
{
	memset(b->cb_buf, 0, b->cb_size);
	b->cb_len = 0;
}

/*
 * rcs_buf_putc()
 *
 * Append a single character <c> to the end of the buffer <b>.
 */
void
rcs_buf_putc(BUF *b, int c)
{
	u_char *bp;

	bp = b->cb_buf + b->cb_len;
	if (bp == (b->cb_buf + b->cb_size)) {
		/* extend */
		if (b->cb_flags & BUF_AUTOEXT)
			rcs_buf_grow(b, (size_t)BUF_INCR);
		else
			errx(1, "rcs_buf_putc failed");

		/* the buffer might have been moved */
		bp = b->cb_buf + b->cb_len;
	}
	*bp = (u_char)c;
	b->cb_len++;
}

/*
 * rcs_buf_getc()
 *
 * Return u_char at buffer position <pos>.
 *
 */
u_char
rcs_buf_getc(BUF *b, size_t pos)
{
	return (b->cb_buf[pos]);
}

/*
 * rcs_buf_append()
 *
 * Append <len> bytes of data pointed to by <data> to the buffer <b>.  If the
 * buffer is too small to accept all data, it will attempt to append as much
 * data as possible, or if the BUF_AUTOEXT flag is set for the buffer, it
 * will get resized to an appropriate size to accept all data.
 * Returns the number of bytes successfully appended to the buffer.
 */
size_t
rcs_buf_append(BUF *b, const void *data, size_t len)
{
	size_t left, rlen;
	u_char *bp, *bep;

	bp = b->cb_buf + b->cb_len;
	bep = b->cb_buf + b->cb_size;
	left = bep - bp;
	rlen = len;

	if (left < len) {
		if (b->cb_flags & BUF_AUTOEXT) {
			rcs_buf_grow(b, len - left);
			bp = b->cb_buf + b->cb_len;
		} else
			rlen = bep - bp;
	}

	memcpy(bp, data, rlen);
	b->cb_len += rlen;

	return (rlen);
}

/*
 * rcs_buf_fappend()
 *
 */
size_t
rcs_buf_fappend(BUF *b, const char *fmt, ...)
{
	size_t ret;
	int n;
	char *str;
	va_list vap;

	va_start(vap, fmt);
	n = vasprintf(&str, fmt, vap);
	va_end(vap);

	if (n == -1)
		errx(1, "rcs_buf_fappend: failed to format data");

	ret = rcs_buf_append(b, str, n);
	xfree(str);
	return (ret);
}

/*
 * rcs_buf_len()
 *
 * Returns the size of the buffer that is being used.
 */
size_t
rcs_buf_len(BUF *b)
{
	return (b->cb_len);
}

/*
 * rcs_buf_write_fd()
 *
 * Write the contents of the buffer <b> to the specified <fd>
 */
int
rcs_buf_write_fd(BUF *b, int fd)
{
	u_char *bp;
	size_t len;
	ssize_t ret;

	len = b->cb_len;
	bp = b->cb_buf;

	do {
		ret = write(fd, bp, len);
		if (ret == -1) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			return (-1);
		}

		len -= (size_t)ret;
		bp += (size_t)ret;
	} while (len > 0);

	return (0);
}

/*
 * rcs_buf_write()
 *
 * Write the contents of the buffer <b> to the file whose path is given in
 * <path>.  If the file does not exist, it is created with mode <mode>.
 */
int
rcs_buf_write(BUF *b, const char *path, mode_t mode)
{
	int fd;
 open:
	if ((fd = open(path, O_WRONLY|O_CREAT|O_TRUNC, mode)) == -1) {
		if (errno == EACCES && unlink(path) != -1)
			goto open;
		else
			err(1, "%s", path);
	}

	if (rcs_buf_write_fd(b, fd) == -1) {
		(void)unlink(path);
		errx(1, "rcs_buf_write: rcs_buf_write_fd: `%s'", path);
	}

	if (fchmod(fd, mode) < 0)
		warn("permissions not set on file %s", path);

	(void)close(fd);

	return (0);
}

/*
 * rcs_buf_write_stmp()
 *
 * Write the contents of the buffer <b> to a temporary file whose path is
 * specified using <template> (see mkstemp.3). NB. This function will modify
 * <template>, as per mkstemp
 */
void
rcs_buf_write_stmp(BUF *b, char *template)
{
	int fd;

	if ((fd = mkstemp(template)) == -1)
		err(1, "%s", template);

	rcs_worklist_add(template, &rcs_temp_files);

	if (rcs_buf_write_fd(b, fd) == -1) {
		(void)unlink(template);
		errx(1, "rcs_buf_write_stmp: rcs_buf_write_fd: `%s'", template);
	}

	(void)close(fd);
}

/*
 * rcs_buf_grow()
 *
 * Grow the buffer <b> by <len> bytes.  The contents are unchanged by this
 * operation regardless of the result.
 */
static void
rcs_buf_grow(BUF *b, size_t len)
{
	b->cb_buf = xrealloc(b->cb_buf, 1, b->cb_size + len);
	b->cb_size += len;
}
