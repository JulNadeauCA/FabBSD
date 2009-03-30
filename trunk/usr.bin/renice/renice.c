/*	$OpenBSD: renice.c,v 1.12 2007/03/16 16:36:06 jmc Exp $	*/

/*
 * Copyright (c) 1983, 1989, 1993
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
"@(#) Copyright (c) 1983, 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)renice.c	8.1 (Berkeley) 6/9/93";
#else
static char rcsid[] = "$OpenBSD: renice.c,v 1.12 2007/03/16 16:36:06 jmc Exp $";
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <err.h>
#include <errno.h>

int main(int, char **);
int donice(int, uid_t, int);
void usage(void);

/*
 * Change the priority (nice) of processes
 * or groups of processes which are already
 * running.
 */
int
main(int argc, char *argv[])
{
	int which = PRIO_PROCESS;
	int errs = 0;
	long prio, who = 0;
	char *ep;

	argc--, argv++;
	if (argc < 2)
		usage();
	prio = strtol(*argv, &ep, 10);
	if (*ep != NULL)
		usage();
	argc--, argv++;
	if (prio > PRIO_MAX)
		prio = PRIO_MAX;
	if (prio < PRIO_MIN)
		prio = PRIO_MIN;
	for (; argc > 0; argc--, argv++) {
		if (strcmp(*argv, "-g") == 0) {
			which = PRIO_PGRP;
			continue;
		}
		if (strcmp(*argv, "-u") == 0) {
			which = PRIO_USER;
			continue;
		}
		if (strcmp(*argv, "-p") == 0) {
			which = PRIO_PROCESS;
			continue;
		}
		if (which == PRIO_USER) {
			struct passwd *pwd = getpwnam(*argv);
			
			if (pwd == NULL) {
				warnx("%s: unknown user", *argv);
				continue;
			}
			who = pwd->pw_uid;
		} else {
			who = strtol(*argv, &ep, 10);
			if (*ep != NULL || who < 0) {
				warnx("%s: bad value", *argv);
				continue;
			}
		}
		errs += donice(which, (uid_t)who, (int)prio);
	}
	exit(errs != 0);
}

int
donice(int which, uid_t who, int prio)
{
	int oldprio;

	errno = 0, oldprio = getpriority(which, who);
	if (oldprio == -1 && errno) {
		warn("getpriority: %d", who);
		return (1);
	}
	if (setpriority(which, who, prio) < 0) {
		warn("setpriority: %d", who);
		return (1);
	}
	printf("%d: old priority %d, new priority %d\n", who, oldprio, prio);
	return (0);
}

void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s priority [[-g] pgrp ...] [[-p] pid ...] "
	    "[[-u] user ...]\n", __progname);
	exit(1);
}
