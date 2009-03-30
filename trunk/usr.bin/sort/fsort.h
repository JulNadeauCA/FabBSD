/*	$OpenBSD: fsort.h,v 1.10 2007/03/13 17:33:58 millert Exp $	*/

/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Peter McIlroy.
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
 *	@(#)fsort.h	8.1 (Berkeley) 6/6/93
 */

#define POW 20			/* exponent for buffer size */
#define BUFSIZE (1 << POW)
#define MAXNUM (BUFSIZE/10)	/* lowish guess at average record size */
#define BUFFEND (EOF-2)
#define BUFFSMALL (EOF-3)	/* buffer is too small to hold line */
#define MAXFCT 1000
#define MAXLLEN ((1 << min(POW-4, 16)) - 14)

extern u_char *linebuf;
extern size_t linebuf_size;

/* temp files in the stack have a file descriptor, a largest bin (maxb)
 * which becomes the last non-empty bin (lastb) when the actual largest
 * bin is smaller than max(half the total file, BUFSIZE)
 * Max_o is the offset of maxb so it can be sought after the other bins
 * are sorted.
*/
struct tempfile {
	FILE *fp;
	u_char maxb;
	u_char lastb;
	int max_o;
};
extern struct tempfile fstack[MAXFCT];
