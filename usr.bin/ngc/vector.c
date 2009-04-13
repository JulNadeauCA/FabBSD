/*	$FabBSD$	*/
/*
 * Copyright (c) 2009 Hypertriton, Inc. <http://hypertriton.com/>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Utility routines.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>

#include "ngc.h"

/* Set V to the zero vector. */
void
ngc_vec_zero(ngc_vec_t *V)
{
	int i;

	for (i = 0; i < NGC_NAXES; i++)
		V->v[i] = 0.0;
}

/* Return the sum of two ngc_vec_t. */
ngc_vec_t
ngc_vec_add(const ngc_vec_t *A, const ngc_vec_t *B)
{
	ngc_vec_t C;
	int i;

	for (i = 0; i < NGC_NAXES; i++) {
		C.v[i] = A->v[i]+B->v[i];
	}
	return (C);
}

/* Return the product of two ngc_vec_t. */
ngc_vec_t
ngc_vec_mult(const ngc_vec_t *A, const ngc_vec_t *B)
{
	ngc_vec_t C;
	int i;

	for (i = 0; i < NGC_NAXES; i++) {
		C.v[i] = A->v[i]*B->v[i];
	}
	return (C);
}

/* Print a ngc_vec_t to a string buffer. */
void
ngc_vec_print(char *s, size_t len, const ngc_vec_t *v)
{
	char num[32];
	int i;

	if (len < 1) {
		return;
	}
	s[0] = '\0';
	for (i = 0; i < NGC_NAXES; i++) {
		snprintf(num, sizeof(num), "%.04f", v->v[i]);
		strlcat(s, num, len);
		if (i < NGC_NAXES-1) 
			strlcat(s, ",", len);
	}
}
