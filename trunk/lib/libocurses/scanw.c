/*	$OpenBSD: scanw.c,v 1.4 2005/08/14 17:15:19 espie Exp $ */
/*
 * Copyright (c) 1981, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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

/*
 * scanw and friends.
 */

#include <stdarg.h>

#include "curses.h"

/*
 * scanw --
 *	Implement a scanf on the standard screen.
 */
int
scanw(const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = vwscanw(stdscr, fmt, ap);
	va_end(ap);
	return (ret);
}

/*
 * wscanw --
 *	Implements a scanf on the given window.
 */
int
wscanw(WINDOW *win, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = vwscanw(win, fmt, ap);
	va_end(ap);
	return (ret);
}

/*
 * mvscanw, mvwscanw -- 
 *	Implement the mvscanw commands.  Due to the variable number of
 *	arguments, they cannot be macros.  Another sigh....
 */
int
mvscanw(register int y, register int x, const char *fmt,...)
{
	va_list ap;
	int ret;

	if (move(y, x) != OK)
		return (ERR);
	va_start(ap, fmt);
	ret = vwscanw(stdscr, fmt, ap);
	va_end(ap);
	return (ret);
}

int
mvwscanw(register WINDOW * win, register int y, register int x,
    const char *fmt, ...)
{
	va_list ap;
	int ret;

	if (move(y, x) != OK)
		return (ERR);
	va_start(ap, fmt);
	ret = vwscanw(win, fmt, ap);
	va_end(ap);
	return (ret);
}

/*
 * vwscanw --
 *	This routine actually executes the scanf from the window.
 */
int
vwscanw(win, fmt, ap)
	WINDOW *win;
	const char *fmt;
	va_list ap;
{

	char buf[1024];

	return (wgetstr(win, buf) == OK ?
	    vsscanf(buf, fmt, ap) : ERR);
}
