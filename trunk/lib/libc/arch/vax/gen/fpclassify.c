/*	$OpenBSD: fpclassify.c,v 1.1 2008/07/24 09:31:07 martynas Exp $	*/
/*
 * Copyright (c) 2008 Martynas Venckus <martynas@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <machine/vaxfp.h>
#include <math.h>

int
__fpclassify(double d)
{
	struct vax_d_floating *p = (struct vax_d_floating *)&d;

	if (p->dflt_exp == 0) {
		return FP_ZERO;
	}

	return FP_NORMAL;
}

int
__fpclassifyf(float f)
{
	struct vax_f_floating *p = (struct vax_f_floating *)&f;

	if (p->fflt_exp == 0) {
		return FP_ZERO;
	}

	return FP_NORMAL;
}
