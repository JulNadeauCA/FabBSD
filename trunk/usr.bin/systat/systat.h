/*	$OpenBSD: systat.h,v 1.9 2008/06/12 22:26:01 canacar Exp $	*/
/*	$NetBSD: systat.h,v 1.2 1995/01/20 08:52:14 jtc Exp $	*/

/*-
 * Copyright (c) 1980, 1989, 1992, 1993
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
 *
 *	@(#)systat.h	8.1 (Berkeley) 6/6/93
 */

#ifndef _SYSTAT_H_
#define _SYSTAT_H_

#include <sys/cdefs.h>
#include <fcntl.h>
#include <kvm.h>
#include "engine.h"


#define	CF_INIT		0x1		/* been initialized */
#define	CF_LOADAV	0x2		/* display w/ load average */

#define	TCP	0x1
#define	UDP	0x2

#define KREAD(addr, buf, len)  kvm_ckread((addr), (buf), (len))
#define NVAL(indx)  namelist[(indx)].n_value
#define NPTR(indx)  (void *)NVAL((indx))
#define NREAD(indx, buf, len) kvm_ckread(NPTR((indx)), (buf), (len))
int kvm_ckread(void *, void *, size_t);

extern char	**dr_name;
extern char	hostname[];
extern double	avenrun[3];
extern kvm_t	*kd;
extern long	ntext;
extern int	*dk_select;
extern int	dk_ndrive;
extern int	hz, stathz;
extern double	naptime;
extern size_t	nhosts;
extern size_t	nports;
extern int	protos;
extern int	verbose;
extern int	nflag;

struct inpcb;

void die(void);
int print_header(void);
int keyboard_callback(int);
int initnetstat(void);
int initifstat(void);
int initiostat(void);
int initsensors(void);
int initmembufs(void);
int initpigs(void);
int initswap(void);
int initvmstat(void);

void error(const char *fmt, ...);
void nlisterr(struct nlist []);

#endif
