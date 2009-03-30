/*	$OpenBSD: fgetws.c,v 1.2 2005/08/08 08:05:36 espie Exp $	*/
/* $NetBSD: fgetws.c,v 1.1 2003/03/07 07:11:37 tshiozak Exp $ */

/*-
 * Copyright (c) 2002 Tim J. Robbins.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Original version ID:
 * FreeBSD: src/lib/libc/stdio/fgetws.c,v 1.4 2002/09/20 13:25:40 tjr Exp
 *
 */

#include <errno.h>
#include <stdio.h>
#include <wchar.h>
#include "local.h"

wchar_t *
fgetws(ws, n, fp)
	wchar_t * __restrict ws;
	int n;
	FILE * __restrict fp;
{
	wchar_t *wsp;
	wint_t wc;

	flockfile(fp);
	_SET_ORIENTATION(fp, 1);

	if (n <= 0) {
		errno = EINVAL;
		goto error;
	}

	wsp = ws;
	while (n-- > 1) {
		if ((wc = __fgetwc_unlock(fp)) == WEOF && errno == EILSEQ) {
			goto error;
		}
		if (wc == WEOF) {
			if (wsp == ws) {
				/* EOF/error, no characters read yet. */
				goto error;
			}
			break;
		}
		*wsp++ = (wchar_t)wc;
		if (wc == L'\n') {
			break;
		}
	}

	*wsp++ = L'\0';
	funlockfile(fp);

	return (ws);

error:
	funlockfile(fp);
	return (NULL);
}
