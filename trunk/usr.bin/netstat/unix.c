/*	$OpenBSD: unix.c,v 1.13 2007/12/19 01:47:00 deraadt Exp $	*/
/*	$NetBSD: unix.c,v 1.13 1995/10/03 21:42:48 thorpej Exp $	*/

/*-
 * Copyright (c) 1983, 1988, 1993
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

/*
 * Display protocol blocks in the unix domain.
 */
#include <sys/param.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/mbuf.h>
#include <sys/sysctl.h>
#include <sys/un.h>
#include <sys/unpcb.h>
#define _KERNEL
struct uio;
struct proc;
#include <sys/file.h>

#include <netinet/in.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <kvm.h>
#include "netstat.h"

static	void unixdomainpr(struct socket *, caddr_t);

static struct	file *file, *fileNFILE;
static int	nfiles;
extern	kvm_t *kvmd;

void
unixpr(u_long off)
{
	struct file *fp;
	struct socket sock, *so = &sock;
	char *filebuf;
	struct protosw *unixsw = (struct protosw *)off;

	filebuf = kvm_getfiles(kvmd, KERN_FILE, 0, &nfiles);
	if (filebuf == NULL) {
		printf("Out of memory (file table).\n");
		return;
	}
	file = (struct file *)(filebuf + sizeof(fp));
	fileNFILE = file + nfiles;
	for (fp = file; fp < fileNFILE; fp++) {
		if (fp->f_count == 0 || fp->f_type != DTYPE_SOCKET)
			continue;
		if (kread((u_long)fp->f_data, so, sizeof (*so)))
			continue;
		/* kludge */
		if (so->so_proto >= unixsw && so->so_proto <= unixsw + 2)
			if (so->so_pcb)
				unixdomainpr(so, fp->f_data);
	}
}

static	char *socktype[] =
    { "#0", "stream", "dgram", "raw", "rdm", "seqpacket" };

static void
unixdomainpr(struct socket *so, caddr_t soaddr)
{
	struct unpcb unpcb, *unp = &unpcb;
	struct mbuf mbuf, *m;
	struct sockaddr_un *sa = NULL;
	static int first = 1;

	if (kread((u_long)so->so_pcb, unp, sizeof (*unp)))
		return;
	if (unp->unp_addr) {
		m = &mbuf;
		if (kread((u_long)unp->unp_addr, m, sizeof (*m)))
			m = NULL;
		sa = (struct sockaddr_un *)(m->m_dat);
	} else
		m = NULL;
	if (first) {
		printf("Active UNIX domain sockets\n");
		printf("%-*.*s %-6.6s %-6.6s %-6.6s %*.*s %*.*s %*.*s %*.*s Addr\n",
		    PLEN, PLEN, "Address", "Type", "Recv-Q", "Send-Q",
		    PLEN, PLEN, "Inode", PLEN, PLEN, "Conn",
		    PLEN, PLEN, "Refs", PLEN, PLEN, "Nextref");
		first = 0;
	}
	printf("%*p %-6.6s %6ld %6ld %*p %*p %*p %*p",
	    PLEN, soaddr, socktype[so->so_type], so->so_rcv.sb_cc,
	    so->so_snd.sb_cc, PLEN, unp->unp_vnode, PLEN, unp->unp_conn,
	    PLEN, unp->unp_refs, PLEN, unp->unp_nextref);
	if (m)
		printf(" %.*s",
		    (int)(m->m_len - (int)(sizeof(*sa) - sizeof(sa->sun_path))),
		    sa->sun_path);
	putchar('\n');
}
