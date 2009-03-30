/*	$OpenBSD: pwd.c,v 1.10 2006/04/01 18:08:48 deraadt Exp $	*/
/*	$NetBSD: pwd.c,v 1.7 1995/03/21 09:08:18 cgd Exp $	*/

/*
 * Copyright (c) 1991, 1993, 1994
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
static char copyright[] =
"@(#) Copyright (c) 1991, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)pwd.c	8.3 (Berkeley) 4/1/94";
#else
static char rcsid[] = "$OpenBSD: pwd.c,v 1.10 2006/04/01 18:08:48 deraadt Exp $";
#endif
#endif /* not lint */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

extern	char *__progname;

void usage(void);

int
main(int argc, char *argv[])
{
	int ch;
	char *p;

	/*
	 * Flags for pwd are a bit strange.  The POSIX 1003.2B/D9 document
	 * has an optional -P flag for physical, which is what this program
	 * will produce by default.  The logical flag, -L, should fail, as
	 * there's no way to display a logical path after forking.  We don't
	 * document either flag, only adding -P for future portability.
	 */
	while ((ch = getopt(argc, argv, "P")) != -1)
		switch (ch) {
		case 'P':
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 0)
		usage();

	if ((p = getcwd(NULL, (size_t)0)) == NULL)
		err(1, "getcwd");
	(void)printf("%s\n", p);
	exit(0);
}

void
usage(void)
{
	(void)fprintf(stderr, "usage: %s\n", __progname);
	exit(1);
}
