/*	$OpenBSD: fpgetsticky.c,v 1.1 2005/04/01 10:54:27 mickey Exp $	*/

/*
 * Written by Miodrag Vallat.  Public domain
 */

#include <sys/types.h>
#include <ieeefp.h>

fp_except
fpgetsticky()
{
	u_int64_t fpsr;

	__asm__ __volatile__("fstd %%fr0,0(%1)" : "=m" (fpsr) : "r" (&fpsr));
	return ((fpsr >> 59) & 0x1f);
}
