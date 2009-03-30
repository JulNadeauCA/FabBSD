/*	$OpenBSD: config.c,v 1.8 2007/03/20 03:50:39 tedu Exp $	*/
/*	$NetBSD: config.c,v 1.7 1995/09/28 06:05:21 tls Exp $	*/

/*
 * Copyright (c) 1989, 1993, 1995
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
#if 0
static char sccsid[] = "@(#)config.c	8.8 (Berkeley) 1/31/95";
#else
static char rcsid[] = "$OpenBSD: config.c,v 1.8 2007/03/20 03:50:39 tedu Exp $";
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "pathnames.h"

struct _head head;

/*
 * config --
 *
 * Read the configuration file and build a doubly linked
 * list that looks like:
 *
 *	tag1 <-> record <-> record <-> record
 *	|
 *	tag2 <-> record <-> record <-> record
 */
void
config(char *fname)
{
	TAG *tp;
	ENTRY *ep;
	FILE *cfp;
	size_t len;
	int lcnt;
	char *p, *t;

	if (fname == NULL)
		fname = _PATH_MANCONF;
	if ((cfp = fopen(fname, "r")) == NULL)
		err(1, "%s", fname);
	TAILQ_INIT(&head);
	for (lcnt = 1; (p = fgetln(cfp, &len)) != NULL; ++lcnt) {
		if (len == 1)			/* Skip empty lines. */
			continue;
		if (p[len - 1] != '\n') {	/* Skip corrupted lines. */
			warnx("%s: line %d corrupted", fname, lcnt);
			continue;
		}
		p[len - 1] = '\0';		/* Terminate the line. */

						/* Skip leading space. */
		while (isspace(*p))
			p++;
						/* Skip empty/comment lines. */
		if (*p == '\0' || *p == '#')
			continue;
						/* Find first token. */
		for (t = p; *t && !isspace(*t); ++t)
			continue;
		if (*t == '\0')			/* Need more than one token.*/
			continue;
		*t = '\0';

		for (tp = TAILQ_FIRST(&head);	/* Find any matching tag. */
		    tp != NULL && strcmp(p, tp->s); tp = TAILQ_NEXT(tp, q));

		if (tp == NULL)		/* Create a new tag. */
			tp = addlist(p);

		/*
		 * Attach new records.  The keyword _build takes the rest of
		 * the line as a single entity, everything else is white
		 * space separated.  The reason we're not just using strtok(3)
		 * for all of the parsing is so we don't get caught if a line
		 * has only a single token on it.
		 */
		if (!strcmp(p, "_build")) {
			while (isspace(*++t));
			if ((ep = malloc(sizeof(ENTRY))) == NULL ||
			    (ep->s = strdup(t)) == NULL)
				err(1, NULL);
			TAILQ_INSERT_TAIL(&tp->list, ep, q);
		} else for (++t; (p = strtok(t, " \t\n")) != NULL; t = NULL) {
			if ((ep = malloc(sizeof(ENTRY))) == NULL ||
			    (ep->s = strdup(p)) == NULL)
				err(1, NULL);
			TAILQ_INSERT_TAIL(&tp->list, ep, q);
		}
	}

	fclose(cfp);
}

/*
 * addlist --
 *	Add a tag to the list.
 */
TAG *
addlist(char *name)
{
	TAG *tp;

	if ((tp = calloc(1, sizeof(TAG))) == NULL ||
	    (tp->s = strdup(name)) == NULL)
		err(1, NULL);
	TAILQ_INIT(&tp->list);
	TAILQ_INSERT_TAIL(&head, tp, q);
	return (tp);
}

/*
 * getlist --
 *	Return the linked list of entries for a tag if it exists.
 */
TAG *
getlist(char *name)
{
	TAG *tp;

	TAILQ_FOREACH(tp, &head, q)
		if (!strcmp(name, tp->s))
			return (tp);
	return (NULL);
}
