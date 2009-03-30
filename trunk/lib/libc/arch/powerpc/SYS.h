/*	$OpenBSD: SYS.h,v 1.13 2002/10/07 04:16:33 drahn Exp $	*/
/*-
 * Copyright (c) 1994
 *	Andrew Cagney.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	from: @(#)SYS.h	8.1 (Berkeley) 6/4/93
 */

#include <sys/syscall.h>

/* r0 will be a non zero errno if there was an error, while r3/r4 will
   contain the return value */

#include "machine/asm.h"

#ifdef __STDC__
#define _CONCAT(x,y)	x##y
#define PSEUDO_PREFIX(p,x,y)	.extern _ASM_LABEL(__cerror) ; \
			ENTRY(p##x) \
				li 0, SYS_##y ; \
				/* sc */
#else /* !__STDC__ */
#define _CONCAT(x,y)	x/**/y
#define PSEUDO_PREFIX(p,x,y)	.extern _ASM_LABEL(__cerror) ; \
			ENTRY(p/**/x) \
				li 0, SYS_/**/y ; \
				/* sc */
#endif /* !__STDC__ */
#define PSEUDO_SUFFIX		cmpwi 0, 0 ; \
				beqlr+ ; \
				b PIC_PLT(_ASM_LABEL(__cerror))

#define PSEUDO_NOERROR_SUFFIX	blr

#define SUFFIX			PSEUDO_SUFFIX

#define ALIAS(x,y)		.weak y; .set y,_CONCAT(x,y);
		
#define PREFIX(x)		ALIAS(_thread_sys_,x) \
				PSEUDO_PREFIX(_thread_sys_,x,x)
#define PREFIX2(x,y)		ALIAS(_thread_sys_,x) \
				PSEUDO_PREFIX(_thread_sys_,x,y)
#define	PSEUDO_NOERROR(x,y)	ALIAS(_thread_sys_,x) \
				PSEUDO_PREFIX(_thread_sys_,x,y) ; \
				sc ; \
				PSEUDO_NOERROR_SUFFIX

#define	PSEUDO(x,y)		ALIAS(_thread_sys_,x) \
				PSEUDO_PREFIX(_thread_sys_,x,y) ; \
				sc ; \
				PSEUDO_SUFFIX

#define RSYSCALL(x)		PSEUDO(x,x)
