/*	$OpenBSD: mathimpl.h,v 1.8 2008/06/21 08:26:19 martynas Exp $	*/
/*	$NetBSD: mathimpl.h,v 1.1 1995/10/10 23:36:31 ragge Exp $	*/
/*
 * Copyright (c) 1988, 1993
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
 *	@(#)mathimpl.h	8.1 (Berkeley) 6/4/93
 */

#include <sys/cdefs.h>

#if defined(__vax__)

/* Deal with concatenation in cpp */
#  define	cat3(a,b,c) a ## b ## c

/* Deal with vax byte order issues */
#  ifdef __vax__
#    define	cat3t(a,b,c) cat3(a,b,c)
#  else
#    define	cat3t(a,b,c) cat3(a,c,b)
#  endif

#  define vccast(name) (*(const double *)(cat3(name,,x)))

   /*
    * Define a constant to high precision on a Vax or Tahoe.
    *
    * Args are the name to define, the decimal floating point value,
    * four 16-bit chunks of the float value in hex
    * (because the vax differ in float format!), the power
    * of 2 of the hex-float exponent, and the hex-float mantissa.
    * Most of these arguments are not used at compile time; they are
    * used in a post-check to make sure the constants were compiled
    * correctly.
    *
    * People who want to use the constant will have to do their own
    *     #define foo vccast(foo)
    * since CPP cannot do this for them from inside another macro (sigh).
    * We define "vccast" if this needs doing.
    */
#  define vc(name, value, x1,x2,x3,x4, bexp, xval) \
	const static long cat3(name,,x)[] = {cat3t(0x,x1,x2), cat3t(0x,x3,x4)};

#  define ic(name, value, bexp, xval) ;

#else	/* defined(__vax__) */

   /* Hooray, we have an IEEE machine */
#  undef vccast
#  define vc(name, value, x1,x2,x3,x4, bexp, xval) ;

#  define ic(name, value, bexp, xval) \
	const static double name = value;

#endif	/* defined(__vax__) */


/*
 * Functions internal to the math package, yet not static.
 */
extern double	__exp__E();
extern double	__log__L();

struct Double {double a, b;};
double __exp__D(double, double);
struct Double __log__D(double);
