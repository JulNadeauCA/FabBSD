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

const char *ngc_modenames[] = {
	"milling",
	"turning"
};

/* Look up a word by address and value. */
struct ngc_word *
ngc_getcmd(const struct ngc_block *blk, char addr, int n)
{
	struct ngc_word *w;

	TAILQ_FOREACH(w, &blk->words, words) {
		if (w->type == addr &&
		    w->data.i == n)
			return (w);
	}
	return (NULL);
}

/* Look up a parameter word. */
struct ngc_word *
ngc_getparam(const struct ngc_block *blk, char addr)
{
	struct ngc_word *w;

	TAILQ_FOREACH(w, &blk->words, words) {
		if (w->type == addr)
			return (w);
	}
	return (NULL);
}

/*
 * Return a vector for the standard axis words. Unspecified entries are
 * left to their original value. Return total number of axis words
 * specified.
 */
int
ngc_axiswords_to_vec(const struct ngc_block *blk, ngc_vec_t *V)
{
	const char *a;
	struct ngc_word *w;
	int i, nAxes = 0;

	TAILQ_FOREACH(w, &blk->words, words) {
		for (i = 0, a = &ngc_axis_words[0];
		     *a != '\0';
		     i++, a++) {
			if (w->type != *a) {
				continue;
			}
			switch (ngc_axis_types[(int)w->type]) {
			case NGC_LINEAR:
				V->v[i] = ngc_units_to_mm(w->data.f);
				break;
			case NGC_ROTARY:
				V->v[i] = w->data.f;		/* Degs */
				break;
			}
			nAxes++;
			break;
		}
	}
	return (nAxes);
}

/* Strip newlines from a string and NUL-terminate. */
void
ngc_nltonul(char *s, size_t len)
{
	if (s[len-1] == '\n') {
		if (s[len-2] == '\r') {
			s[len-2] = '\0';
		}
		s[len-1] = '\0';
	} else {
		s[len] = '\0';
	}
}

/* Convert a number in currently selected units (ngc_units) to millimeters. */
ngc_real_t
ngc_units_to_mm(ngc_real_t v)
{
	return (ngc_units == NGC_INCH) ? (v*25.4) : v;
}
