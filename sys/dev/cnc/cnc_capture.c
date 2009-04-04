/*	$FabBSD$	*/
/*
 * Copyright (c) 2009 Hypertriton, Inc. <http://www.hypertriton.com/>
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

#ifdef MOTIONCAPTURE

/*
 * Interface for recording motion capture signals. This is useful for
 * simulations and debugging.
 */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/pool.h>
#include <sys/conf.h>
#include <sys/lock.h>
#include <sys/mount.h>
#include <sys/proc.h>

#include <sys/cnc.h>

#include "cncvar.h"
#include "cnc_capturevar.h"

struct cnc_capture_frame cnc_capbuf[CNC_CAPTURE_BUFSIZE];
int                      cnc_capbuf_size = 0;

void
cnc_capture_open(void)
{
	bzero(cnc_capbuf, sizeof(cnc_capbuf));
	cnc_capbuf_size = 0;
	cnc_capture = 1;
	printf("cnc: motion capture enabled (%d frames)\n", CNC_CAPTURE_BUFSIZE);
}

void
cnc_capture_close(void)
{
	cnc_capture = 0;
	printf("cnc: motion capture disabled\n");
}

int
cnc_capture_read(dev_t dev, struct uio *uio, int ioflag)
{
	int rv;

	if (!cnc_capture) {
		return (EIO);
	}
	while (uio->uio_resid > 0) {
		if (!cnc_capture) {
			break;
		}
		if (uio->uio_offset >= cnc_capbuf_size) {
//			printf("uio_offset=%d >= capbuf=%d, waiting\n", uio->uio_offset, cnc_capbuf_size);
			if ((rv = tsleep(cnc_capbuf, PWAIT|PCATCH, "motcaprd", 0))) {
				return (rv);
			}
			uio->uio_offset = 0;
		}
//		printf("uiomove %ld (capbuf[%ld/%ld])\n", (long)cnc_capbuf_size,
//		    (long)uio->uio_offset, (long)cnc_capbuf_size);
		if ((rv = uiomove(cnc_capbuf, cnc_capbuf_size, uio)) != 0) {
			return (rv);
		}
		wakeup(cnc_capbuf);
	}
	return (0);
}

#endif /* MOTIONCAPTURE */
