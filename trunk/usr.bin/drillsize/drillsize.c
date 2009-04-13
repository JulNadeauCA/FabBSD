/*
 * Copyright (c) 2009 Hypertriton, Inc. <http://hypertriton.com/>
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>

struct drillsize {
	const char *name;
	double v;
};

#include "drills_fractional.h"
#include "drills_letter.h"
#include "drills_wireno.h"

int Rflag = 0;
int Pflag = 0;
int Pangle = 0;
double inches = 0.0;

static void
printusage(void)
{
	extern char *__progname;
	printf("Usage: %s [-R] [-p angle] "
	       "[-i inches] | [-l letter] | [-m mm] | [-w wireno]\n",
	       __progname);
	exit(1);
}

static const char *
lookup(double in, const struct drillsize *tbl, int n)
{
	int i;

	for (i = 0; i < n; i++) {
		const struct drillsize *ds = &tbl[i];
		if (ds->v == in)
			return (ds->name);
	}
	return ("-");
}

static double
lookup_wireno(const char *s)
{
	int i;

	for (i = 0; i < ndrills_wireno; i++) {
		const struct drillsize *ds = &drills_wireno[i];
		if (strcasecmp(ds->name, s) == 0)
			return (ds->v);
	}
	return (0.0);
}

static double
lookup_letter(const char *s)
{
	int i;

	for (i = 0; i < ndrills_letter; i++) {
		const struct drillsize *ds = &drills_letter[i];
		if (strcasecmp(ds->name, s) == 0)
			return (ds->v);
	}
	return (0.0);
}

int
main(int argc, char *argv[])
{
	char *ep, *s, c, ch;
	const char *err;
	char *letter = NULL, *wireno = NULL;
	int pt, i;

	while ((ch = getopt(argc, argv, "Rp:i:l:m:w:?h")) != -1) {
		switch (ch) {
		case 'R':
			Rflag = 1;
			break;
		case 'p':
			Pflag = 1;
			Pangle = (int)strtonum(optarg, 1, 180, &err);
			if (err != NULL) { errx(1, "-p: %s", err); }
			break;
		case 'i':
			if ((s = strchr(optarg, '/')) != NULL) {
				int num = atoi(optarg);
				int den = atoi(s+1);

				if (s == '\0' || den == 0) { errx(1, "-i: invalid fraction"); }
				inches = (double)num/den;
			} else {
				inches = strtod(optarg, &ep);
				if (optarg[0] == '\0' || *ep != '\0') { errx(1, "-i: invalid value"); }
			}
			break;
		case 'l':
			if ((c = optarg[0]) != '\0' && optarg[1] == '\0' &&
			    toupper(c) >= 65 && toupper(c) <= 90) {
				letter = strdup(optarg);
			} else {
				errx(1, "-l: invalid letter drill size");
			}
			break;
		case 'w':
			wireno = strdup(optarg);
			break;
		case 'm':
			inches = strtod(optarg,&ep)/25.4;
			if (optarg[0] == '\0' || *ep != '\0') {
				errx(1, "-i: invalid value");
			}
			break;
		default:
			printusage();
		}
	}

	if (wireno) {
		inches = lookup_wireno(wireno);
	} else if (letter) {
		inches = lookup_letter(letter);
	} else {
		if (inches == 0.0)
			printusage();
	}
	printf("Inches:     %.4f\n", inches);
	printf("Fractional: %s\n", lookup(inches, drills_fractional, ndrills_fractional));
	printf("Wire no.:   %s\n", lookup(inches, drills_wireno, ndrills_wireno));
	printf("Letter:     %s\n", lookup(inches, drills_letter, ndrills_letter));

	return (0);
}

