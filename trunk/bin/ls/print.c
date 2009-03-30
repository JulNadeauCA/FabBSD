/*	$OpenBSD: print.c,v 1.25 2007/05/07 18:39:28 millert Exp $	*/
/*	$NetBSD: print.c,v 1.15 1996/12/11 03:25:39 thorpej Exp $	*/

/*
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Michael Fischbein.
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
static char sccsid[] = "@(#)print.c	8.5 (Berkeley) 7/28/94";
#else
static char rcsid[] = "$OpenBSD: print.c,v 1.25 2007/05/07 18:39:28 millert Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fts.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <tzfile.h>
#include <unistd.h>
#include <util.h>

#include "ls.h"
#include "extern.h"

static int	printaname(FTSENT *, u_long, u_long);
static void	printlink(FTSENT *);
static void	printsize(size_t, off_t);
static void	printtime(time_t);
static int	printtype(u_int);
static int	compute_columns(DISPLAY *, int *);

#define	IS_NOPRINT(p)	((p)->fts_number == NO_PRINT)

void
printscol(DISPLAY *dp)
{
	FTSENT *p;

	for (p = dp->list; p; p = p->fts_link) {
		if (IS_NOPRINT(p))
			continue;
		(void)printaname(p, dp->s_inode, dp->s_block);
		(void)putchar('\n');
	}
}

void
printlong(DISPLAY *dp)
{
	struct stat *sp;
	FTSENT *p;
	NAMES *np;
	char buf[20];

	if (dp->list->fts_level != FTS_ROOTLEVEL && (f_longform || f_size))
		(void)printf("total %lu\n", howmany(dp->btotal, blocksize));

	for (p = dp->list; p; p = p->fts_link) {
		if (IS_NOPRINT(p))
			continue;
		sp = p->fts_statp;
		if (f_inode)
			(void)printf("%*u ", dp->s_inode, sp->st_ino);
		if (f_size)
			(void)printf("%*qd ",
			    dp->s_block, howmany(sp->st_blocks, blocksize));
		(void)strmode(sp->st_mode, buf);
		np = p->fts_pointer;
                (void)printf("%s %*u ", buf, dp->s_nlink, sp->st_nlink);
		if (!f_grouponly)
			(void)printf("%-*s  ", dp->s_user, np->user);
		(void)printf("%-*s  ", dp->s_group, np->group);
		if (f_flags)
			(void)printf("%-*s ", dp->s_flags, np->flags);
		if (S_ISCHR(sp->st_mode) || S_ISBLK(sp->st_mode))
			(void)printf("%3d, %3d ",
			    major(sp->st_rdev), minor(sp->st_rdev));
		else if (dp->bcfile)
			(void)printf("%*s%*qd ",
			    8 - dp->s_size, "", dp->s_size, sp->st_size);
		else
			printsize(dp->s_size, sp->st_size);	
		if (f_accesstime)
			printtime(sp->st_atime);
		else if (f_statustime)
			printtime(sp->st_ctime);
		else
			printtime(sp->st_mtime);
		(void)putname(p->fts_name);
		if (f_type || (f_typedir && S_ISDIR(sp->st_mode)))
			(void)printtype(sp->st_mode);
		if (S_ISLNK(sp->st_mode))
			printlink(p);
		(void)putchar('\n');
	}
}

static int
compute_columns(DISPLAY *dp, int *pnum)
{
	int colwidth;
	extern int termwidth;
	int mywidth;

	colwidth = dp->maxlen;
	if (f_inode)
		colwidth += dp->s_inode + 1;
	if (f_size)
		colwidth += dp->s_block + 1;
	if (f_type || f_typedir)
		colwidth += 1;

	colwidth += 1;
	mywidth = termwidth + 1;	/* no extra space for last column */

	if (mywidth < 2 * colwidth) {
		printscol(dp);
		return (0);
	}

	*pnum = mywidth / colwidth;
	return (mywidth / *pnum);		/* spread out if possible */
}

void
printcol(DISPLAY *dp)
{
	static FTSENT **array;
	static int lastentries = -1;
	FTSENT *p;
	int base, chcnt, col, colwidth, num;
	int numcols, numrows, row;

	if ((colwidth = compute_columns(dp, &numcols)) == 0)
		return;
	/*
	 * Have to do random access in the linked list -- build a table
	 * of pointers.
	 */
	if (dp->entries > lastentries) {
		FTSENT **a;

		if ((a = realloc(array, dp->entries * sizeof(FTSENT *))) ==
		    NULL) {
			free(array);
			array = NULL;
			dp->entries = 0;
			lastentries = -1;
			warn(NULL);
			printscol(dp);
			return;
		}
		lastentries = dp->entries;
		array = a;
	}
	for (p = dp->list, num = 0; p; p = p->fts_link)
		if (p->fts_number != NO_PRINT)
			array[num++] = p;

	numrows = num / numcols;
	if (num % numcols)
		++numrows;

	if (dp->list->fts_level != FTS_ROOTLEVEL && (f_longform || f_size))
		(void)printf("total %lu\n", howmany(dp->btotal, blocksize));
	for (row = 0; row < numrows; ++row) {
		for (base = row, col = 0;;) {
			chcnt = printaname(array[base], dp->s_inode, dp->s_block);
			if ((base += numrows) >= num)
				break;
			if (++col == numcols)
				break;
			while (chcnt++ < colwidth)
				putchar(' ');
		}
		(void)putchar('\n');
	}
}

/*
 * print [inode] [size] name
 * return # of characters printed, no trailing characters.
 */
static int
printaname(FTSENT *p, u_long inodefield, u_long sizefield)
{
	struct stat *sp;
	int chcnt;

	sp = p->fts_statp;
	chcnt = 0;
	if (f_inode)
		chcnt += printf("%*u ", (int)inodefield, sp->st_ino);
	if (f_size)
		chcnt += printf("%*qd ",
		    (int)sizefield, howmany(sp->st_blocks, blocksize));
	chcnt += putname(p->fts_name);
	if (f_type || (f_typedir && S_ISDIR(sp->st_mode)))
		chcnt += printtype(sp->st_mode);
	return (chcnt);
}

static void
printtime(time_t ftime)
{
	int i;
	char *longstring;

	longstring = ctime(&ftime);
	for (i = 4; i < 11; ++i)
		(void)putchar(longstring[i]);

#define	SIXMONTHS	((DAYSPERNYEAR / 2) * SECSPERDAY)
	if (f_sectime)
		for (i = 11; i < 24; i++)
			(void)putchar(longstring[i]);
	else if (ftime + SIXMONTHS > time(NULL))
		for (i = 11; i < 16; ++i)
			(void)putchar(longstring[i]);
	else {
		(void)putchar(' ');
		for (i = 20; i < 24; ++i)
			(void)putchar(longstring[i]);
	}
	(void)putchar(' ');
}

void
printacol(DISPLAY *dp)
{
	FTSENT *p;
	int chcnt, col, colwidth;
	int numcols;

	if ( (colwidth = compute_columns(dp, &numcols)) == 0)
		return;

	if (dp->list->fts_level != FTS_ROOTLEVEL && (f_longform || f_size))
		(void)printf("total %lu\n", howmany(dp->btotal, blocksize));
	col = 0;
	for (p = dp->list; p; p = p->fts_link) {
		if (IS_NOPRINT(p))
			continue;
		if (col >= numcols) {
			col = 0;
			(void)putchar('\n');
		}
		chcnt = printaname(p, dp->s_inode, dp->s_block);
		col++;
		if (col < numcols)
			while (chcnt++ < colwidth)
				(void)putchar(' ');
	}
	(void)putchar('\n');
}

void
printstream(DISPLAY *dp)
{
	extern int termwidth;
	FTSENT *p;
	int col;
	int extwidth;

	extwidth = 0;
	if (f_inode)
		extwidth += dp->s_inode + 1;
	if (f_size)
		extwidth += dp->s_block + 1;
	if (f_type)
		extwidth += 1;

	for (col = 0, p = dp->list; p != NULL; p = p->fts_link) {
		if (IS_NOPRINT(p))
			continue;
		if (col > 0) {
			(void)putchar(','), col++;
			if (col + 1 + extwidth + p->fts_namelen >= termwidth)
				(void)putchar('\n'), col = 0;
			else
				(void)putchar(' '), col++;
		}
		col += printaname(p, dp->s_inode, dp->s_block);
	}
	(void)putchar('\n');
}

static int
printtype(u_int mode)
{
	switch (mode & S_IFMT) {
	case S_IFDIR:
		(void)putchar('/');
		return (1);
	case S_IFIFO:
		(void)putchar('|');
		return (1);
	case S_IFLNK:
		(void)putchar('@');
		return (1);
	case S_IFSOCK:
		(void)putchar('=');
		return (1);
	}
	if (mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
		(void)putchar('*');
		return (1);
	}
	return (0);
}

static void
printlink(FTSENT *p)
{
	int lnklen;
	char name[MAXPATHLEN], path[MAXPATHLEN];

	if (p->fts_level == FTS_ROOTLEVEL)
		(void)snprintf(name, sizeof(name), "%s", p->fts_name);
	else
		(void)snprintf(name, sizeof(name),
		    "%s/%s", p->fts_parent->fts_accpath, p->fts_name);
	if ((lnklen = readlink(name, path, sizeof(path) - 1)) == -1) {
		(void)fprintf(stderr, "\nls: %s: %s\n", name, strerror(errno));
		return;
	}
	path[lnklen] = '\0';
	(void)printf(" -> ");
	(void)putname(path);
}

static void
printsize(size_t width, off_t bytes)
{
	char ret[FMT_SCALED_STRSIZE];

	if ((f_humanval) && (fmt_scaled(bytes, ret) != -1)) {
		(void)printf("%*s ", (u_int)width, ret);
		return;
	}
	(void)printf("%*qd ", (u_int)width, bytes);
}
