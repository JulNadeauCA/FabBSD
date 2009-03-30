/*	$OpenBSD: rs.c,v 1.18 2007/09/02 15:19:34 deraadt Exp $	*/

/*-
 * Copyright (c) 1993
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

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static const char sccsid[] = "@(#)rs.c	8.1 (Berkeley) 6/6/93";
#else
static const char rcsid[] = "$OpenBSD: rs.c,v 1.18 2007/09/02 15:19:34 deraadt Exp $";
#endif
#endif /* not lint */

/*
 *	rs - reshape a data array
 *	Author:  John Kunze, Office of Comp. Affairs, UCB
 *		BEWARE: lots of unfinished edges
 */

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

long	flags;
#define	TRANSPOSE	000001
#define	MTRANSPOSE	000002
#define	ONEPERLINE	000004
#define	ONEISEPONLY	000010
#define	ONEOSEPONLY	000020
#define	NOTRIMENDCOL	000040
#define	SQUEEZE		000100
#define	SHAPEONLY	000200
#define	DETAILSHAPE	000400
#define	RIGHTADJUST	001000
#define	NULLPAD		002000
#define	RECYCLE		004000
#define	SKIPPRINT	010000
#define ONEPERCHAR	0100000
#define NOARGS		0200000

short	*colwidths;
int	nelem;
char	**elem;
char	**endelem;
char	*curline;
int	allocsize = BUFSIZ;
int	curlen;
int	irows, icols;
int	orows, ocols;
int	maxlen;
int	skip;
int	propgutter;
char	isep = ' ', osep = ' ';
int	owidth = 80, gutter = 2;

void	  usage(void);
void	  getargs(int, char *[]);
void	  getfile(void);
int	  getline(void);
char	**getptrs(char **);
void	  prepfile(void);
void	  prints(char *, int);
void	  putfile(void);

#define INCR(ep) do {			\
	if (++ep >= endelem)		\
		ep = getptrs(ep);	\
} while(0)

int
main(int argc, char *argv[])
{
	getargs(argc, argv);
	getfile();
	if (flags & SHAPEONLY) {
		printf("%d %d\n", irows, icols);
		exit(0);
	}
	prepfile();
	putfile();
	exit(0);
}

void
getfile(void)
{
	char *p;
	char *endp;
	char **ep = NULL;
	int multisep = (flags & ONEISEPONLY ? 0 : 1);
	int nullpad = flags & NULLPAD;
	char **padto;

	while (skip--) {
		getline();
		if (flags & SKIPPRINT)
			puts(curline);
	}
	getline();
	if (flags & NOARGS && curlen < owidth)
		flags |= ONEPERLINE;
	if (flags & ONEPERLINE)
		icols = 1;
	else				/* count cols on first line */
		for (p = curline, endp = curline + curlen; p < endp; p++) {
			if (*p == isep && multisep)
				continue;
			icols++;
			while (*p && *p != isep)
				p++;
		}
	ep = getptrs(elem);
	p = curline;
	do {
		if (flags & ONEPERLINE) {
			*ep = curline;
			INCR(ep);		/* prepare for next entry */
			if (maxlen < curlen)
				maxlen = curlen;
			irows++;
			continue;
		}
		for (p = curline, endp = curline + curlen; p < endp; p++) {
			if (*p == isep && multisep)
				continue;	/* eat up column separators */
			if (*p == isep)		/* must be an empty column */
				*ep = "";
			else			/* store column entry */
				*ep = p;
			while (p < endp && *p != isep)
				p++;		/* find end of entry */
			*p = '\0';		/* mark end of entry */
			if (maxlen < p - *ep)	/* update maxlen */
				maxlen = p - *ep;
			INCR(ep);		/* prepare for next entry */
		}
		irows++;			/* update row count */
		if (nullpad) {			/* pad missing entries */
			padto = elem + irows * icols;
			while (ep < padto) {
				*ep = "";
				INCR(ep);
			}
		}
	} while (getline() != EOF);
	*ep = NULL;				/* mark end of pointers */
	nelem = ep - elem;
}

void
putfile(void)
{
	char **ep;
	int i, j, n;

	ep = elem;
	if (flags & TRANSPOSE) {
		for (i = 0; i < orows; i++) {
			for (j = i; j < nelem; j += orows)
				prints(ep[j], (j - i) / orows);
			putchar('\n');
		}
	} else {
		for (n = 0, i = 0; i < orows && n < nelem; i++) {
			for (j = 0; j < ocols; j++) {
				if (n++ >= nelem)
					break;
				prints(*ep++, j);
			}
			putchar('\n');
		}
	}
}

void
prints(char *s, int col)
{
	int n;
	char *p = s;

	while (*p)
		p++;
	n = (flags & ONEOSEPONLY ? 1 : colwidths[col] - (p - s));
	if (flags & RIGHTADJUST)
		while (n-- > 0)
			putchar(osep);
	for (p = s; *p; p++)
		putchar(*p);
	while (n-- > 0)
		putchar(osep);
}

void
usage(void)
{
	extern char *__progname;

	fprintf(stderr,
	    "usage: %s [-CcSs[x]] [-KkGgw N] [-EeHhjmnTtyz] [rows [cols]]\n",
	    __progname);
	exit(1);
}

void
prepfile(void)
{
	char **ep;
	int  i;
	int  j;
	char **lp;
	int colw;
	int max = 0;
	int n;

	if (!nelem)
		exit(0);
	gutter += maxlen * propgutter / 100.0;
	colw = maxlen + gutter;
	if (flags & MTRANSPOSE) {
		orows = icols;
		ocols = irows;
	}
	else if (orows == 0 && ocols == 0) {	/* decide rows and cols */
		ocols = owidth / colw;
		if (ocols == 0) {
			warnx("Display width %d is less than column width %d",
			    owidth, colw);
			ocols = 1;
		}
		if (ocols > nelem)
			ocols = nelem;
		orows = nelem / ocols + (nelem % ocols ? 1 : 0);
	}
	else if (orows == 0)			/* decide on rows */
		orows = nelem / ocols + (nelem % ocols ? 1 : 0);
	else if (ocols == 0)			/* decide on cols */
		ocols = nelem / orows + (nelem % orows ? 1 : 0);
	lp = elem + orows * ocols;
	while (lp > endelem) {
		getptrs(elem + nelem);
		lp = elem + orows * ocols;
	}
	if (flags & RECYCLE) {
		for (ep = elem + nelem; ep < lp; ep++)
			*ep = *(ep - nelem);
		nelem = lp - elem;
	}
	if (!(colwidths = (short *) calloc(ocols, sizeof(short))))
		errx(1, "malloc:  No gutter space");
	if (flags & SQUEEZE) {
		if (flags & TRANSPOSE)
			for (ep = elem, i = 0; i < ocols; i++) {
				for (j = 0; j < orows; j++)
					if ((n = strlen(*ep++)) > max)
						max = n;
				colwidths[i] = max + gutter;
			}
		else
			for (ep = elem, i = 0; i < ocols; i++) {
				for (j = i; j < nelem; j += ocols)
					if ((n = strlen(ep[j])) > max)
						max = n;
				colwidths[i] = max + gutter;
			}
	} else {
		for (i = 0; i < ocols; i++)
			colwidths[i] = colw;
	}
	if (!(flags & NOTRIMENDCOL)) {
		if (flags & RIGHTADJUST)
			colwidths[0] -= gutter;
		else
			colwidths[ocols - 1] = 0;
	}
	n = orows * ocols;
	if (n > nelem && (flags & RECYCLE))
		nelem = n;
}

#define	BSIZE	2048
char	ibuf[BSIZE];		/* two screenfuls should do */

int
getline(void)	/* get line; maintain curline, curlen; manage storage */
{
	static	int putlength;
	static	char *endblock = ibuf + BSIZE;
	char *p;
	int c, i;

	if (!irows) {
		curline = ibuf;
		putlength = flags & DETAILSHAPE;
	}
	else if (skip <= 0) {			/* don't waste storage */
		curline += curlen + 1;
		if (putlength)		/* print length, recycle storage */
			printf(" %d line %d\n", curlen, irows);
	}
	if (!putlength && endblock - curline < BUFSIZ) {   /* need storage */
		if (!(curline = (char *) malloc(BSIZE)))
			errx(1, "File too large");
		endblock = curline + BSIZE;
	}
	for (p = curline, i = 1; i < BUFSIZ; *p++ = c, i++)
		if ((c = getchar()) == EOF || c == '\n')
			break;
	*p = '\0';
	curlen = i - 1;
	return(c);
}

char **
getptrs(char **sp)
{
	char **p;
	int newsize, gap;

	newsize = allocsize * 2;
	p = realloc(elem, newsize * sizeof(char *));
	if (p == NULL)
		err(1, "no memory");

	gap = p - elem;
	elem = p;
	allocsize = newsize;
	sp += gap;
	endelem = elem + allocsize;
	return(sp);
}

void
getargs(int ac, char *av[])
{
	int ch;
	const char *errstr;

	if (ac == 1)
		flags |= NOARGS | TRANSPOSE;
	while ((ch = getopt(ac, av, "c::C::s::S::k:K:g:G:w:tTeEnyjhHmz")) != -1) {
		switch (ch) {
		case 'T':
			flags |= MTRANSPOSE;
			/* FALLTHROUGH */
		case 't':
			flags |= TRANSPOSE;
			break;
		case 'c':		/* input col. separator */
			flags |= ONEISEPONLY;
			/* FALLTHROUGH */
		case 's':		/* one or more allowed */
			if (optarg == NULL)
				isep = '\t';	/* default is ^I */
			else if (optarg[1] != '\0')
				usage();	/* single char only */
			else
				isep = *optarg;
			break;
		case 'C':
			flags |= ONEOSEPONLY;
			/* FALLTHROUGH */
		case 'S':
			if (optarg == NULL)
				osep = '\t';	/* default is ^I */
			else if (optarg[1] != '\0')
				usage();	/* single char only */
			else
				osep = *optarg;
			break;
		case 'w':		/* window width, default 80 */
			owidth = strtonum(optarg, 1, INT_MAX, &errstr);
			if (errstr) {
				warnx("width %s", errstr);
				usage();
			}
			break;
		case 'K':			/* skip N lines */
			flags |= SKIPPRINT;
			/* FALLTHROUGH */
		case 'k':			/* skip, do not print */
			skip = strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr) {
				warnx("skip value %s", errstr);
				usage();
			}
			if (skip == 0)
				skip = 1;
			break;
		case 'm':
			flags |= NOTRIMENDCOL;
			break;
		case 'g':		/* gutter width */
			gutter = strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr) {
				warnx("gutter width %s", errstr);
				usage();
			}
			break;
		case 'G':
			propgutter = strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr) {
				warnx("gutter proportion %s", errstr);
				usage();
			}
			break;
		case 'e':		/* each line is an entry */
			flags |= ONEPERLINE;
			break;
		case 'E':
			flags |= ONEPERCHAR;
			break;
		case 'j':			/* right adjust */
			flags |= RIGHTADJUST;
			break;
		case 'n':	/* null padding for missing values */
			flags |= NULLPAD;
			break;
		case 'y':
			flags |= RECYCLE;
			break;
		case 'H':			/* print shape only */
			flags |= DETAILSHAPE;
			/* FALLTHROUGH */
		case 'h':
			flags |= SHAPEONLY;
			break;
		case 'z':			/* squeeze col width */
			flags |= SQUEEZE;
			break;
		default:
			usage();
		}
	}
	ac -= optind;
	av += optind;

	switch (ac) {
	case 2:
		ocols = strtonum(av[1], 0, INT_MAX, &errstr);
		if (errstr) {
			warnx("columns value %s", errstr);
			usage();
		}
		/* FALLTHROUGH */
	case 1:
		orows = strtonum(av[0], 0, INT_MAX, &errstr);
		if (errstr) {
			warnx("columns value %s", errstr);
			usage();
		}
		/* FALLTHROUGH */
	case 0:
		break;
	default:
		usage();
	}
}
