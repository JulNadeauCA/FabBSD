/*	$OpenBSD: kvm_private.h,v 1.11 2006/03/31 03:59:40 deraadt Exp $ */
/*	$NetBSD: kvm_private.h,v 1.7 1996/05/05 04:32:15 gwr Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software developed by the Computer Systems
 * Engineering group at Lawrence Berkeley Laboratory under DARPA contract
 * BG 91-66 and contributed to Berkeley.
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
 *	@(#)kvm_private.h	8.1 (Berkeley) 6/4/93
 */

struct __kvm {
	/*
	 * a string to be prepended to error messages
	 * provided for compatibility with sun's interface
	 * if this value is null, errors are saved in errbuf[]
	 */
	const char *program;
	char	*errp;		/* XXX this can probably go away */
	char	errbuf[_POSIX2_LINE_MAX];
	DB	*db;
	int	pmfd;		/* physical memory file (or crashdump) */
	int	vmfd;		/* virtual memory file (-1 if crashdump) */
	int	swfd;		/* swap file (e.g., /dev/drum) */
	int	nlfd;		/* namelist file (e.g., /vmunix) */
	struct kinfo_proc *procbase;
	struct kinfo_proc2 *procbase2;
	int	nbpg;		/* page size */
	char	*swapspc;	/* (dynamic) storage for swapped pages */
	char	*argspc, *argbuf; /* (dynamic) storage for argv strings */
	int	arglen;		/* length of the above */
	char	**argv;		/* (dynamic) storage for argv pointers */
	int	argc;		/* length of above (not actual # present) */

	/*
	 * Header structures for kernel dumps. Only gets filled in for
	 * dead kernels.
	 */
	struct kcore_hdr	*kcore_hdr;
	size_t	cpu_dsize;
	void	*cpu_data;
	off_t	dump_off;	/* Where the actual dump starts	*/

	/*
	 * Kernel virtual address translation state.  This only gets filled
	 * in for dead kernels; otherwise, the running kernel (i.e. kmem)
	 * will do the translations for us.  It could be big, so we
	 * only allocate it if necessary.
	 */
	struct vmstate *vmst; /* XXX: should become obsoleted */
	/*
	 * These kernel variables are used for looking up user addresses,
	 * and are cached for efficiency.
	 */
	struct pglist *vm_page_buckets;
	int vm_page_hash_mask;
	int alive;	/* Dead or alive. */
#define ISALIVE(kd) ((kd)->alive)
};

/*
 * Functions used internally by kvm, but across kvm modules.
 */
void	 _kvm_err(kvm_t *kd, const char *program, const char *fmt, ...);
int	 _kvm_dump_mkheader(kvm_t *kd_live, kvm_t *kd_dump);
void	 _kvm_freeprocs(kvm_t *kd);
void	 _kvm_freevtop(kvm_t *);
int	 _kvm_initvtop(kvm_t *);
int	 _kvm_kvatop(kvm_t *, u_long, paddr_t *);
void	*_kvm_malloc(kvm_t *kd, size_t);
off_t	 _kvm_pa2off(kvm_t *, paddr_t);
void	*_kvm_realloc(kvm_t *kd, void *, size_t);
void	 _kvm_syserr(kvm_t *kd, const char *program, const char *fmt, ...);
ssize_t	 _kvm_pread(kvm_t *, int, void *, size_t, off_t);
ssize_t	 _kvm_pwrite(kvm_t *, int, const void *, size_t, off_t);
