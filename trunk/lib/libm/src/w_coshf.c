/* w_coshf.c -- float version of w_cosh.c.
 * Conversion to float by Ian Lance Taylor, Cygnus Support, ian@cygnus.com.
 */

/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
 */

#if defined(LIBM_SCCS) && !defined(lint)
static char rcsid[] = "$NetBSD: w_coshf.c,v 1.3 1995/05/10 20:48:49 jtc Exp $";
#endif

/* 
 * wrapper coshf(x)
 */

#include "math.h"
#include "math_private.h"

float
coshf(float x)		/* wrapper coshf */
{
#ifdef _IEEE_LIBM
	return __ieee754_coshf(x);
#else
	float z;
	z = __ieee754_coshf(x);
	if(_LIB_VERSION == _IEEE_ || isnanf(x)) return z;
	if(fabsf(x)>(float)8.9415985107e+01) {	
		/* cosh overflow */
	        return (float)__kernel_standard((double)x,(double)x,105);
	} else
	    return z;
#endif
}
