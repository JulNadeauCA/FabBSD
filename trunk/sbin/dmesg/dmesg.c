/*	$OpenBSD: dmesg.c,v 1.20 2006/12/24 00:47:27 djm Exp $	*/
/*	$NetBSD: dmesg.c,v 1.8 1995/03/18 14:54:49 cgd Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/msgbuf.h>
#include <sys/sysctl.h>

#include <err.h>
#include <fcntl.h>
#include <kvm.h>
#include <limits.h>
#include <nlist.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <vis.h>

struct nlist nl[] = {
#define	X_MSGBUF	0
	{ "_msgbufp" },
	{ NULL },
};

void usage(void);

#define	KREAD(addr, var) \
	kvm_read(kd, addr, &var, sizeof(var)) != sizeof(var)

int
main(int argc, char *argv[])
{
	int ch, newl, skip, i;
	char *p;
	struct msgbuf cur;
	char *memf, *nlistf, *bufdata;
	char buf[5];

	memf = nlistf = NULL;
	while ((ch = getopt(argc, argv, "M:N:")) != -1)
		switch(ch) {
		case 'M':
			memf = optarg;
			break;
		case 'N':
			nlistf = optarg;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (memf == NULL && nlistf == NULL) {
		int mib[2], msgbufsize;
		size_t len;

		mib[0] = CTL_KERN;
		mib[1] = KERN_MSGBUFSIZE;
		len = sizeof(msgbufsize);
		if (sysctl(mib, 2, &msgbufsize, &len, NULL, 0))
			err(1, "sysctl: KERN_MSGBUFSIZE");

		msgbufsize += sizeof(struct msgbuf) - 1;
		bufdata = malloc(msgbufsize);
		if (bufdata == NULL)
			errx(1, "couldn't allocate space for buffer data");

		memset(bufdata, 0, msgbufsize);
		mib[1] = KERN_MSGBUF;
		len = msgbufsize;
		if (sysctl(mib, 2, bufdata, &len, NULL, 0))
			err(1, "sysctl: KERN_MSGBUF");

		memcpy(&cur, bufdata, sizeof(cur));
		bufdata = ((struct msgbuf *)bufdata)->msg_bufc;
	} else {
#ifndef NOKVM
		struct msgbuf *bufp;
		kvm_t *kd;

		/* Read in kernel message buffer, do sanity checks. */
		if ((kd = kvm_open(nlistf, memf, NULL, O_RDONLY,
		    "dmesg")) == NULL)
			return (1);

		if (kvm_nlist(kd, nl) == -1)
			errx(1, "kvm_nlist: %s", kvm_geterr(kd));
		if (nl[X_MSGBUF].n_type == 0)
			errx(1, "%s: msgbufp not found",
			    nlistf ? nlistf : "namelist");
		if (KREAD(nl[X_MSGBUF].n_value, bufp))
			errx(1, "kvm_read: %s: (0x%lx)", kvm_geterr(kd),
			    nl[X_MSGBUF].n_value);
		if (KREAD((long)bufp, cur))
			errx(1, "kvm_read: %s (%0lx)", kvm_geterr(kd),
			    (unsigned long)bufp);
		if (cur.msg_magic != MSG_MAGIC)
			errx(1, "magic number incorrect");
		bufdata = malloc(cur.msg_bufs);
		if (bufdata == NULL)
			errx(1, "couldn't allocate space for buffer data");
		if (kvm_read(kd, (long)&bufp->msg_bufc, bufdata,
		    cur.msg_bufs) != cur.msg_bufs)
			errx(1, "kvm_read: %s", kvm_geterr(kd));
		kvm_close(kd);
#endif
	}

	if (cur.msg_bufx >= cur.msg_bufs)
		cur.msg_bufx = 0;
	/*
	 * The message buffer is circular; start at the read pointer, and
	 * go to the write pointer - 1.
	 */
	for (newl = skip = i = 0, p = bufdata + cur.msg_bufx;
	    i < cur.msg_bufs; i++, p++) {
		if (p == bufdata + cur.msg_bufs)
			p = bufdata;
		ch = *p;
		/* Skip "\n<.*>" syslog sequences. */
		if (skip) {
			if (ch == '>')
				newl = skip = 0;
			continue;
		}
		if (newl && ch == '<') {
			skip = 1;
			continue;
		}
		if (ch == '\0')
			continue;
		newl = ch == '\n';
		vis(buf, ch, 0, 0);
		if (buf[1] == 0)
			putchar(buf[0]);
		else
			printf("%s", buf);
	}
	if (!newl)
		putchar('\n');
	return (0);
}

void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-M core] [-N system]\n", __progname);
	exit(1);
}
