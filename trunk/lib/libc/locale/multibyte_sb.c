/*	$OpenBSD: multibyte_sb.c,v 1.6 2005/12/10 02:01:51 deraadt Exp $	*/
/*	$NetBSD: multibyte_sb.c,v 1.4 2003/08/07 16:43:04 agc Exp $	*/

/*
 * Copyright (c) 1991 The Regents of the University of California.
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

#include <errno.h>
#include <stdlib.h>
#include <wchar.h>

/*
 * Stub multibyte character functions.
 * This cheezy implementation is fixed to the native single-byte
 * character set.
 */

/*ARGSUSED*/
int
mbsinit(const mbstate_t *ps)
{

	return 1;
}

/*ARGSUSED*/
size_t
mbrlen(const char *s, size_t n, mbstate_t *ps)
{

	/* ps appears to be unused */

	if (s == NULL || *s == '\0')
		return 0;
	if (n == 0)
		return (size_t)-1;
	return 1;
}

int
mblen(const char *s, size_t n)
{

	/* s may be NULL */

	return mbrlen(s, n, NULL);
}

/*ARGSUSED*/
size_t
wcrtomb(char *s, wchar_t wchar, mbstate_t *ps)
{

	/* s may be NULL */
	/* ps appears to be unused */

	if (s == NULL)
		return 0;

	*s = (char) wchar;
	return 1;
}

int
wctomb(char *s, wchar_t wchar)
{

	/* s may be NULL */

	return wcrtomb(s, wchar, NULL);
}

/*ARGSUSED*/
size_t
mbsrtowcs(wchar_t *pwcs, const char **s, size_t n, mbstate_t *ps)
{
	int count = 0;

	/* pwcs may be NULL */
	/* s may be NULL */
	/* ps appears to be unused */

	if (!s || !*s)
		return 0;

	if (n != 0) {
		if (pwcs != NULL) {
			do {
				if ((*pwcs++ = (wchar_t)(unsigned char)*(*s)++) == 0)
					break;
				count++;
			} while (--n != 0);
		} else {
			do {
				if (((wchar_t)*(*s)++) == 0)
					break;
				count++;
			} while (--n != 0);
		}
	}
	
	return count;
}

size_t
mbstowcs(wchar_t *pwcs, const char *s, size_t n)
{

	/* pwcs may be NULL */
	/* s may be NULL */

	return mbsrtowcs(pwcs, &s, n, NULL);
}

/*ARGSUSED*/
size_t
wcsrtombs(char *s, const wchar_t **pwcs, size_t n, mbstate_t *ps)
{
	int count = 0;

	/* s may be NULL */
	/* pwcs may be NULL */
	/* ps appears to be unused */

	if (pwcs == NULL || *pwcs == NULL)
		return (0);

	if (s == NULL) {
		while (*(*pwcs)++ != 0)
			count++;
		return(count);
	}

	if (n != 0) {
		do {
			if ((*s++ = (char) *(*pwcs)++) == 0)
				break;
			count++;
		} while (--n != 0);
	}

	return count;
}

size_t
wcstombs(char *s, const wchar_t *pwcs, size_t n)
{

	/* s may be NULL */
	/* pwcs may be NULL */

	return wcsrtombs(s, &pwcs, n, NULL);
}

wint_t
btowc(int c)
{
	if (c == EOF || c & ~0xFF)
		return WEOF;
	return (wint_t)c;
}

int
wctob(wint_t c)
{
	if (c == WEOF || c & ~0xFF)
		return EOF;
	return (int)c;
}

int
wcscoll(const wchar_t *s1, const wchar_t *s2)
{
	while (*s1 == *s2++)
		if (*s1++ == 0)
			return (0);
	return ((unsigned char)(*s1) - (unsigned char)(*--s2));
}

size_t 
wcsxfrm(wchar_t *dest, const wchar_t *src, size_t n)
{
	if (n == 0)
		return wcslen(src);
	return wcslcpy(dest, src, n);
}
