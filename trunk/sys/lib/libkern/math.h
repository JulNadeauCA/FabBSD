/*	$FabBSD$	*/
/*	$OpenBSD: math.h,v 1.20 2008/07/24 09:41:58 martynas Exp $	*/
/*	from: @(#)fdlibm.h 5.1 93/09/24 */
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

#ifndef _LIBKERN_MATH_H_
#define _LIBKERN_MATH_H_

#include <sys/_types.h>
#include <sys/cdefs.h>
#include <sys/limits.h>

extern char __infinity[];
#define HUGE_VAL	(*(double *)(void *)__infinity)

typedef	__double_t	double_t;
typedef	__float_t	float_t;

#define	HUGE_VALF	((float)HUGE_VAL)
#define	HUGE_VALL	((long double)HUGE_VAL)
#define	INFINITY	HUGE_VALF
extern char __nan[];
#define	NAN		(*(float *)(void *)__nan)

#define	FP_INFINITE	0x01
#define	FP_NAN		0x02
#define	FP_NORMAL	0x04
#define	FP_SUBNORMAL	0x08
#define	FP_ZERO		0x10

#define FP_ILOGB0	(-INT_MAX)
#define FP_ILOGBNAN	INT_MAX

#define fpclassify(x) ((sizeof (x) == sizeof (float)) ? __fpclassifyf(x) : (sizeof (x) == sizeof (double)) ? __fpclassify(x) : __fpclassifyl(x))
#define isfinite(x) ((sizeof (x) == sizeof (float)) ? __isfinitef(x) : (sizeof (x) == sizeof (double)) ? __isfinite(x) : __isfinitel(x))
#define isinf(x) ((sizeof (x) == sizeof (float)) ? isinff(x) : (sizeof (x) == sizeof (double)) ? __isinf(x) : __isinfl(x))
#define isnan(x) ((sizeof (x) == sizeof (float)) ? isnanf(x) : (sizeof (x) == sizeof (double)) ? __isnan(x) : __isnanl(x))
#define isnormal(x) ((sizeof (x) == sizeof (float)) ? __isnormalf(x) : (sizeof (x) == sizeof (double)) ? __isnormal(x) : __isnormall(x))
#define signbit(x) ((sizeof (x) == sizeof (float)) ? __signbitf(x) : (sizeof (x) == sizeof (double)) ? __signbit(x) : __signbitl(x))

#define	isgreater(x, y)		(!isunordered((x), (y)) && (x) > (y))
#define	isgreaterequal(x, y)	(!isunordered((x), (y)) && (x) >= (y))
#define	isless(x, y)		(!isunordered((x), (y)) && (x) < (y))
#define	islessequal(x, y)	(!isunordered((x), (y)) && (x) <= (y))
#define	islessgreater(x, y)	(!isunordered((x), (y)) && ((x) > (y) || (y) > (x)))
#define	isunordered(x, y)	(isnan(x) || isnan(y))

#define	M_E		2.7182818284590452354	/* e */
#define	M_LOG2E		1.4426950408889634074	/* log 2e */
#define	M_LOG10E	0.43429448190325182765	/* log 10e */
#define	M_LN2		0.69314718055994530942	/* log e2 */
#define	M_LN10		2.30258509299404568402	/* log e10 */
#define	M_PI		3.14159265358979323846	/* pi */
#define	M_PI_2		1.57079632679489661923	/* pi/2 */
#define	M_PI_4		0.78539816339744830962	/* pi/4 */
#define	M_1_PI		0.31830988618379067154	/* 1/pi */
#define	M_2_PI		0.63661977236758134308	/* 2/pi */
#define	M_2_SQRTPI	1.12837916709551257390	/* 2/sqrt(pi) */
#define	M_SQRT2		1.41421356237309504880	/* sqrt(2) */
#define	M_SQRT1_2	0.70710678118654752440	/* 1/sqrt(2) */

/*
 * set X_TLOSS = pi*2**52, which is possibly defined in <values.h>
 * (one may replace the following line by "#include <values.h>")
 */
#define X_TLOSS		1.41484755040568800000e+16

#define	DOMAIN		1
#define	SING		2
#define	OVERFLOW	3
#define	UNDERFLOW	4
#define	TLOSS		5
#define	PLOSS		6

__BEGIN_DECLS
extern double copysign(double, double);
extern double cos(double);
extern double sin(double);
extern double tan(double);
extern double ieee754_log(double);
extern double ieee754_log10(double);
extern double fabs(double);
extern double sqrt(double);
extern double cbrt(double);
extern double modf(double, double *);
extern double scalbn(double, int);
extern double ceil(double);
extern double floor(double);

extern float copysignf(float, float);
extern float cosf(float);
extern float sinf(float);
extern float tanf(float);
extern float ieee754_logf(float);
extern float ieee754_log10f(float);
extern float fabsf(float);
extern float sqrtf(float);
extern float cbrtf(float);
extern float scalbnf(float, int);
extern float ceilf(float);
extern float floorf(float);
__END_DECLS

#endif /* !_LIBKERN_MATH_H_ */
