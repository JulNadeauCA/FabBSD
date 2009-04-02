/*	$FabBSD$	*/
/*
 * Copyright (c) 2009 Hypertriton, Inc. <http://hypertriton.com/>
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
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/cnc.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "pathnames.h"

int
main(int argc, char *argv)
{
	int fd, wfd, capmode;
	ssize_t nr, nw, off;
	static size_t bufsize;
	char *buf;
	struct stat sb;

	if ((fd = open(_PATH_DEV_CNC, O_RDONLY)) == -1) {
		err(1, "%s", _PATH_DEV_CNC);
	}
	wfd = fileno(stdout);
	if (fstat(wfd, &sb)) {
		err(1, "stdout");
	}
	bufsize = MAX(sb.st_blksize, BUFSIZ);
	if ((buf = malloc(bufsize)) == NULL)
		err(1, "malloc");

	capmode = 1;
	if (ioctl(fd, CNC_SETCAPTUREMODE, &capmode) == -1) {
		err(1, "%s: CNC_SETCAPTUREMODE", _PATH_DEV_CNC);
	}
	while ((nr = read(fd, buf, bufsize)) != -1 && nr != 0) {
		for (off = 0;
		     nr != 0;
		     nr -= nw, off += nw) {
			if ((nw = write(wfd, &buf[off], (size_t)nr)) == 0 ||
			     nw == -1) {
				warn("write error on stdout");
				goto fail;
			}
		}
	}
	if (nr < 0) {
		warn("read error on %s", _PATH_DEV_CNC);
		goto fail;
	}

	capmode = 0;
	ioctl(fd, CNC_SETCAPTUREMODE, &capmode);
	close(fd);
	return (0);
fail:
	capmode = 0;
	ioctl(fd, CNC_SETCAPTUREMODE, &capmode);
	close(fd);
	return (0);
}

