/*	$OpenBSD: wwrint.c,v 1.8 2005/04/13 02:33:09 deraadt Exp $	*/
/*	$NetBSD: wwrint.c,v 1.4 1995/12/21 10:46:24 mycroft Exp $	*/

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
static char sccsid[] = "@(#)wwrint.c	8.1 (Berkeley) 6/6/93";
#else
static char rcsid[] = "$OpenBSD: wwrint.c,v 1.8 2005/04/13 02:33:09 deraadt Exp $";
#endif
#endif /* not lint */

#include "ww.h"
#include "tt.h"
#include <sys/types.h>
#include <unistd.h>

/*
 * Tty input interrupt handler.
 * (1) Read input into buffer (wwib*).
 * (2) Set the interrupt flag if anything is read.
 * Currently, the last is used to get out of the blocking
 * select() in wwiomux().
 * To avoid race conditions, we only modify wwibq in here, except
 * when the buffer is empty; and everywhere else, we only change wwibp.
 * It should be completely safe.
 */
void
wwrint()
{
	int n;

	wwnread++;
	n = read(STDIN_FILENO, wwibq, wwibe - wwibq);
	if (n > 0) {
		if (tt.tt_rint)
			n = (*tt.tt_rint)(wwibq, n);
		if (n > 0) {
			wwibq += n;
			wwnreadc += n;
			/*
			 * Hasten or delay the next checkpoint,
			 * as the case may be.
			 */
			if (tt.tt_checkpoint && !wwdocheckpoint)
				(void) alarm(1);
			wwsetintr();
		}
	} else if (n == 0)
		wwnreadz++;
	else
		wwnreade++;
}
