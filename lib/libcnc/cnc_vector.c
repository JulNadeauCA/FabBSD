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

#include <sys/limits.h>

#include <limits.h>
#include <cnc.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>

/*
 * Parse a string of the form "x,y,z" into a cnc_vec_t.
 * TODO conversions
 */
int
cnc_vec_parse(cnc_vec_t *V, const char *sarg)
{
	char *ep;
	char *sb = strdup(sarg), *s = sb, *sp;
	int i;

	if (sb == NULL) {
		cnc_set_error("Out of memory");
		return (-1);
	}
	for (i = 0; i < CNC_NAXES; i++) {
		V->v[i] = 0;
	}
	for (i = 0;
	     (sp = strsep(&s, ",: ")) && i < CNC_NAXES;
	     i++) {
		errno = 0;
		if (*sp == 'm') {
			*sp = '-';
		}
		V->v[i] = (cnc_pos_t)strtoull(sp, &ep, 10);
		if (sp[0] == '\0' || *ep != '\0') {
			cnc_set_error("No numerical velocity value");
			goto fail;
		}
		if (errno == ERANGE || V->v[i] == ULLONG_MAX) {
			cnc_set_error("Velocity value out of range");
			goto fail;
		}
	}
	free(sb);
	return (0);
fail:
	free(sb);
	return (-1);
}

/* Print a string representation of the given vector. */
int
cnc_vec_print(const cnc_vec_t *V, char *s, size_t len)
{
	char sn[32];
	int i;

	if (len < 1) {
		goto oflow;
	}
	s[0] = '\0';
	for (i = 0; i < CNC_NAXES; i++) {
		snprintf(sn, sizeof(sn), "%lld", (long long)V->v[i]);
		if (strlcat(s, sn, len) >= len)
			goto oflow;
		if (i < CNC_NAXES-1 && strlcat(s, ",", len) >= len)
			goto oflow;
	}
	return (0);
oflow:
	cnc_set_error("overflow");
	return (-1);
}

/* Initialize v to the zero vector. */
void
cnc_vec_zero(cnc_vec_t *v)
{
	int i;

	for (i = 0; i < CNC_NAXES; i++)
		v->v[i] = 0;
}

/* Return (v1-v2) in vDst. */
void
cnc_vec_sub(cnc_vec_t *vDst, const cnc_vec_t *v1, const cnc_vec_t *v2)
{
	int i;

	for (i = 0; i < CNC_NAXES; i++)
		vDst->v[i] = v1->v[i] - v2->v[i];
}

/* Return (v1+v2) in vDst. */
void
cnc_vec_add(cnc_vec_t *vDst, const cnc_vec_t *v1, const cnc_vec_t *v2)
{
	int i;

	for (i = 0; i < CNC_NAXES; i++)
		vDst->v[i] = v1->v[i] + v2->v[i];
}

/* Compute the dot product of two vectors. */
cnc_real_t
cnc_vec_dotprod(const cnc_vec_t *v1, const cnc_vec_t *v2)
{
	cnc_real_t dot = 0.0;
	int i;

	for (i = 0; i < CNC_NAXES; i++) {
		dot += ((cnc_real_t)v1->v[i])*((cnc_real_t)v2->v[i]);
	}
	return (dot);
}

/* Return real length of vector v. */
cnc_real_t
cnc_vec_length(const cnc_vec_t *v)
{
	cnc_real_t len_2 = 0.0;
	int i;

	for (i = 0; i < CNC_NAXES; i++) {
		len_2 += ((cnc_real_t)v->v[i])*((cnc_real_t)v->v[i]);
	}
	return sqrt(len_2);
}

/* Return real distance between vectors v1 and v2. */
cnc_real_t
cnc_vec_distance(const cnc_vec_t *v1, const cnc_vec_t *v2)
{
	cnc_vec_t vd;

	cnc_vec_sub(&vd, v2, v1);
	return cnc_vec_length(&vd);
}

/* Return 0 if v1 and v2 are the same vector. */
int
cnc_vec_compare(const cnc_vec_t *v1, const cnc_vec_t *v2)
{
	int i;

	for (i = 0; i < CNC_NAXES; i++) {
		if (v1->v[i] != v2->v[i])
			return (1);
	}
	return (0);
}
