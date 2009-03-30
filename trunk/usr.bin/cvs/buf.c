/*	$OpenBSD: buf.c,v 1.72 2008/06/10 01:00:34 joris Exp $	*/
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

#include <sys/stat.h>
#include <sys/time.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "atomicio.h"
#include "cvs.h"
#include "buf.h"

#define BUF_INCR	128

struct cvs_buf {
	/* buffer handle, buffer size, and data length */
	u_char	*cb_buf;
	size_t	 cb_size;
	size_t	 cb_len;
};

#define SIZE_LEFT(b)	(b->cb_size - b->cb_len)

static void	cvs_buf_grow(BUF *, size_t);

/*
 * cvs_buf_alloc()
 *
 * Create a new buffer structure and return a pointer to it.  This structure
 * uses dynamically-allocated memory and must be freed with cvs_buf_free(),
 * once the buffer is no longer needed.
 */
BUF *
cvs_buf_alloc(size_t len)
{
	BUF *b;

	b = xmalloc(sizeof(*b));
	/* Postpone creation of zero-sized buffers */
	if (len > 0)
		b->cb_buf = xcalloc(1, len);
	else
		b->cb_buf = NULL;

	b->cb_size = len;
	b->cb_len = 0;

	return (b);
}

BUF *
cvs_buf_load(const char *path)
{
	int fd;
	BUF *bp;

	if ((fd = open(path, O_RDONLY, 0600)) == -1)
		fatal("cvs_buf_load: failed to load '%s' : %s", path,
		    strerror(errno));

	bp = cvs_buf_load_fd(fd);
	(void)close(fd);
	return (bp);
}

BUF *
cvs_buf_load_fd(int fd)
{
	struct stat st;
	BUF *buf;

	if (fstat(fd, &st) == -1)
		fatal("cvs_buf_load_fd: fstat: %s", strerror(errno));

	if (lseek(fd, 0, SEEK_SET) == -1)
		fatal("cvs_buf_load_fd: lseek: %s", strerror(errno));

	if (st.st_size > SIZE_MAX)
		fatal("cvs_buf_load_fd: file size too big");
	buf = cvs_buf_alloc(st.st_size);
	if (atomicio(read, fd, buf->cb_buf, buf->cb_size) != buf->cb_size)
		fatal("cvs_buf_load_fd: read: %s", strerror(errno));
	buf->cb_len = buf->cb_size;

	return (buf);
}

/*
 * cvs_buf_free()
 *
 * Free the buffer <b> and all associated data.
 */
void
cvs_buf_free(BUF *b)
{
	if (b->cb_buf != NULL)
		xfree(b->cb_buf);
	xfree(b);
}

/*
 * cvs_buf_release()
 *
 * Free the buffer <b>'s structural information but do not free the contents
 * of the buffer.  Instead, they are returned and should be freed later using
 * free().
 */
u_char *
cvs_buf_release(BUF *b)
{
	u_char *tmp;

	tmp = b->cb_buf;
	xfree(b);
	return (tmp);
}

/*
 * cvs_buf_empty()
 *
 * Empty the contents of the buffer <b> and reset pointers.
 */
void
cvs_buf_empty(BUF *b)
{
	memset(b->cb_buf, 0, b->cb_size);
	b->cb_len = 0;
}

/*
 * cvs_buf_putc()
 *
 * Append a single character <c> to the end of the buffer <b>.
 */
void
cvs_buf_putc(BUF *b, int c)
{
	u_char *bp;

	bp = b->cb_buf + b->cb_len;
	if (bp == (b->cb_buf + b->cb_size)) {
		/* extend */
		cvs_buf_grow(b, (size_t)BUF_INCR);

		/* the buffer might have been moved */
		bp = b->cb_buf + b->cb_len;
	}
	*bp = (u_char)c;
	b->cb_len++;
}

/*
 * cvs_buf_puts()
 *
 * Wrapper function for constant strings to cvs_buf_append.
 */
void
cvs_buf_puts(BUF *b, const char *str)
{
	cvs_buf_append(b, str, strlen(str));
}

/*
 * cvs_buf_getc()
 *
 * Return u_char at buffer position <pos>.
 *
 */
u_char
cvs_buf_getc(BUF *b, size_t pos)
{
	return (b->cb_buf[pos]);
}

/*
 * cvs_buf_append()
 *
 * Append <len> bytes of data pointed to by <data> to the buffer <b>.  If the
 * buffer is too small to accept all data, it will attempt to append as much
 * data as possible, or if the BUF_AUTOEXT flag is set for the buffer, it
 * will get resized to an appropriate size to accept all data.
 * Returns the number of bytes successfully appended to the buffer.
 */
void
cvs_buf_append(BUF *b, const void *data, size_t len)
{
	size_t left;
	u_char *bp, *bep;

	bp = b->cb_buf + b->cb_len;
	bep = b->cb_buf + b->cb_size;
	left = bep - bp;

	if (left < len) {
		cvs_buf_grow(b, len - left);
		bp = b->cb_buf + b->cb_len;
	}

	memcpy(bp, data, len);
	b->cb_len += len;
}

/*
 * cvs_buf_len()
 *
 * Returns the size of the buffer that is being used.
 */
size_t
cvs_buf_len(BUF *b)
{
	return (b->cb_len);
}

/*
 * cvs_buf_write_fd()
 *
 * Write the contents of the buffer <b> to the specified <fd>
 */
int
cvs_buf_write_fd(BUF *b, int fd)
{
	if (atomicio(vwrite, fd, b->cb_buf, b->cb_len) != b->cb_len)
		return (-1);
	return (0);
}

/*
 * cvs_buf_write()
 *
 * Write the contents of the buffer <b> to the file whose path is given in
 * <path>.  If the file does not exist, it is created with mode <mode>.
 */
int
cvs_buf_write(BUF *b, const char *path, mode_t mode)
{
	int fd;
 open:
	if ((fd = open(path, O_WRONLY|O_CREAT|O_TRUNC, mode)) == -1) {
		if (errno == EACCES && unlink(path) != -1)
			goto open;
		else
			fatal("open: `%s': %s", path, strerror(errno));
	}

	if (cvs_buf_write_fd(b, fd) == -1) {
		(void)unlink(path);
		fatal("cvs_buf_write: cvs_buf_write_fd: `%s'", path);
	}

	if (fchmod(fd, mode) < 0)
		cvs_log(LP_ERR, "permissions not set on file %s", path);

	(void)close(fd);

	return (0);
}

/*
 * cvs_buf_write_stmp()
 *
 * Write the contents of the buffer <b> to a temporary file whose path is
 * specified using <template> (see mkstemp.3). NB. This function will modify
 * <template>, as per mkstemp
 */
int
cvs_buf_write_stmp(BUF *b, char *template, struct timeval *tv)
{
	int fd;

	if ((fd = mkstemp(template)) == -1)
		fatal("mkstemp: `%s': %s", template, strerror(errno));

	if (cvs_buf_write_fd(b, fd) == -1) {
		(void)unlink(template);
		fatal("cvs_buf_write_stmp: cvs_buf_write_fd: `%s'", template);
	}

	if (tv != NULL) {
		if (futimes(fd, tv) == -1)
			fatal("cvs_buf_write_stmp: futimes failed");
	}

	cvs_worklist_add(template, &temp_files);

	if (lseek(fd, 0, SEEK_SET) < 0)
		fatal("cvs_buf_write_stmp: lseek: %s", strerror(errno));

	return (fd);
}

/*
 * cvs_buf_grow()
 *
 * Grow the buffer <b> by <len> bytes.  The contents are unchanged by this
 * operation regardless of the result.
 */
static void
cvs_buf_grow(BUF *b, size_t len)
{
	b->cb_buf = xrealloc(b->cb_buf, 1, b->cb_size + len);
	b->cb_size += len;
}

/*
 * cvs_buf_copy()
 *
 * Copy the first <len> bytes of data in the buffer <b> starting at offset
 * <off> in the destination buffer <dst>, which can accept up to <len> bytes.
 * Returns the number of bytes successfully copied, or -1 on failure.
 */
ssize_t
cvs_buf_copy(BUF *b, size_t off, void *dst, size_t len)
{
	size_t rc;

	if (off > b->cb_len)
		fatal("cvs_buf_copy failed");

	rc = MIN(len, (b->cb_len - off));
	memcpy(dst, b->cb_buf + off, rc);

	return (ssize_t)rc;
}

/*
 * cvs_buf_peek()
 *
 * Peek at the contents of the buffer <b> at offset <off>.
 */
const u_char *
cvs_buf_peek(BUF *b, size_t off)
{
	if (off >= b->cb_len)
		return (NULL);

	return (b->cb_buf + off);
}

int
cvs_buf_differ(const BUF *b1, const BUF *b2)
{
	if (b1->cb_len != b2->cb_len)
		return (1);

	return (memcmp(b1->cb_buf, b2->cb_buf, b1->cb_len));
}
