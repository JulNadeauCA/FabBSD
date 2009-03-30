/*	$OpenBSD: wwadd.c,v 1.6 2003/06/03 02:56:23 millert Exp $	*/
/*	$NetBSD: wwadd.c,v 1.4 1996/02/08 21:48:56 mycroft Exp $	*/

/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Edward Wang at The University of California, Berkeley.
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
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)wwadd.c	8.1 (Berkeley) 6/6/93";
#else
static char rcsid[] = "$OpenBSD: wwadd.c,v 1.6 2003/06/03 02:56:23 millert Exp $";
#endif
#endif /* not lint */

#include "ww.h"

/*
 * Stick w1 behind w2.
 */
wwadd(w1, w2)
struct ww *w1;
struct ww *w2;
{
	int i;
	struct ww *w;

	w1->ww_order = w2->ww_order + 1;
	w1->ww_back = w2;
	w1->ww_forw = w2->ww_forw;
	w2->ww_forw->ww_back = w1;
	w2->ww_forw = w1;

	for (w = w1->ww_forw; w != &wwhead; w = w->ww_forw)
		w->ww_order++;
	for (i = w1->ww_i.t; i < w1->ww_i.b; i++) {
		int j;
		unsigned char *smap = wwsmap[i];
		char *win = w1->ww_win[i];
		union ww_char *ns = wwns[i];
		union ww_char *buf = w1->ww_buf[i];
		int nvis = 0;
		int nchanged = 0;

		for (j = w1->ww_i.l; j < w1->ww_i.r; j++) {
			w = wwindex[smap[j]];
			if (w1->ww_order > w->ww_order)
				continue;
			if (win[j] & WWM_GLS)
				continue;
			if (w != &wwnobody && w->ww_win[i][j] == 0)
				w->ww_nvis[i]--;
			smap[j] = w1->ww_index;
			if (win[j] == 0)
				nvis++;
			ns[j].c_w = buf[j].c_w ^ win[j] << WWC_MSHIFT;
			nchanged++;
		}
		if (nchanged > 0)
			wwtouched[i] |= WWU_TOUCHED;
		w1->ww_nvis[i] = nvis;
	}
}
