/*
 * Written by Michael Shalayeff. Public Domain
 */

#if defined(LIBM_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: s_floorf.c,v 1.4 2003/10/31 23:22:37 mickey Exp $";
#endif

#include <sys/types.h>
#include <machine/ieeefp.h>
#include "math.h"

float
floorf(float x)
{
	u_int64_t ofpsr, fpsr;

	__asm__ __volatile__("fstds %%fr0,0(%1)" : "=m" (ofpsr) : "r" (&ofpsr));
	fpsr = (ofpsr & ~((u_int64_t)FP_RM << (9 + 32))) |
	    ((u_int64_t)FP_RZ << (9 + 32));
	__asm__ __volatile__("fldds 0(%0), %%fr0" :: "r" (&fpsr));

	__asm__ __volatile__("frnd,sgl %0,%0" : "+f" (x));

	__asm__ __volatile__("fldds 0(%0), %%fr0" :: "r" (&ofpsr));
	return (x);
}
