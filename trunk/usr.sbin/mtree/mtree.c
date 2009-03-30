/*	$OpenBSD: mtree.c,v 1.19 2008/05/17 23:31:52 sobrado Exp $	*/
/*	$NetBSD: mtree.c,v 1.7 1996/09/05 23:29:22 thorpej Exp $	*/

/*-
 * Copyright (c) 1989, 1990, 1993
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
"@(#) Copyright (c) 1989, 1990, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static const char sccsid[] = "@(#)mtree.c	8.1 (Berkeley) 6/6/93";
#else
static const char rcsid[] = "$OpenBSD: mtree.c,v 1.19 2008/05/17 23:31:52 sobrado Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <fts.h>
#include "mtree.h"
#include "extern.h"

extern u_int32_t crc_total;

int ftsoptions = FTS_PHYSICAL;
int cflag, dflag, eflag, iflag, lflag, nflag, qflag, rflag, sflag, tflag,
    uflag, Uflag;
u_int keys;
char fullpath[MAXPATHLEN];

static void usage(void);

int
main(int argc, char *argv[])
{
	extern int optind;
	extern char *optarg;
	int ch;
	char *dir, *p;
	int status;

	dir = NULL;
	keys = KEYDEFAULT;
	while ((ch = getopt(argc, argv, "cdef:iK:k:lnp:qrs:tUux")) != -1)
		switch((char)ch) {
		case 'c':
			cflag = 1;
			break;
		case 'd':
			dflag = 1;
			break;
		case 'e':
			eflag = 1;
			break;
		case 'f':
			if (!(freopen(optarg, "r", stdin)))
				error("%s: %s", optarg, strerror(errno));
			break;
		case 'i':
			iflag = 1;
			break;
		case 'K':
			while ((p = strsep(&optarg, " \t,")) != NULL)
				if (*p != '\0')
					keys |= parsekey(p, NULL);
			break;
		case 'k':
			keys = F_TYPE;
			while ((p = strsep(&optarg, " \t,")) != NULL)
				if (*p != '\0')
					keys |= parsekey(p, NULL);
			break;
		case 'l':
			lflag = 1;
			break;
		case 'n':
			nflag = 1;
			break;
		case 'p':
			dir = optarg;
			break;
		case 'q':
			qflag = 1;
			break;
		case 'r':
			rflag = 1;
			break;
		case 's':
			sflag = 1;
			crc_total = ~strtol(optarg, &p, 0);
			if (*p)
				error("illegal seed value -- %s", optarg);
			break;
		case 't':
			tflag = 1;
			break;
		case 'U':
			Uflag = 1;
			uflag = 1;
			break;
		case 'u':
			uflag = 1;
			break;
		case 'x':
			ftsoptions |= FTS_XDEV;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc)
		usage();

	if (dir && chdir(dir))
		error("%s: %s", dir, strerror(errno));

	if ((cflag || sflag) && !getcwd(fullpath, sizeof fullpath))
		error("getcwd: %s", strerror(errno));

	if (lflag == 1 && uflag == 1)
		error("-l and -u flags are mutually exclusive");

	if (cflag) {
		cwalk();
		exit(0);
	}
	status = verify();
	if (Uflag && (status == MISMATCHEXIT))
		status = 0;
	exit(status);
}

static void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: mtree [-cdeilnqrtUux] [-f spec] [-K keywords] "
	    "[-k keywords] [-p path]\n"
	    "             [-s seed]\n");
	exit(1);
}
