/*
 * Copyright (c) 2009-2011 Hypertriton, Inc. <http://hypertriton.com/>
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

#include "drills_letter.h"
#include "drills_wireno.h"

int Rflag = 0;
int Pflag = 0;
int Pangle = 0;
double inches = 0.0;
const int maxFrac = 64;
int undersize = 0;
int oversize = 0;

static void
printusage(void)
{
	extern char *__progname;
	printf("Usage: %s [-ouR] [-p angle] "
	       "[-i inches] | [-l letter] | [-m mm] | [-w wireno]\n",
	       __progname);
	exit(1);
}

static char *
lookuptbl(double in, const struct drillsize *tbl, int n)
{
	const struct drillsize *ds, *dsNear;
	char *s;
	int i;
	
	if ((s = malloc(64)) == NULL) {
		return (NULL);
	}
	s[0] = '-';
	s[1] = '\0';

	if (undersize) {
		for (i = 0; i < n; i++) {
			ds = &tbl[i];
			if (i > 0 && ds->v >= in) {
				dsNear = &tbl[i-1];
				snprintf(s, 64, "%s (%.04f) -%.04f (undersize)",
				    dsNear->name, dsNear->v,
				    fabs(in - dsNear->v));
				break;
			}
		}
	} else if (oversize) {
		for (i = n-1; i > 0; i--) {
			ds = &tbl[i];
			if (i < n-1 && ds->v <= in) {
				dsNear = &tbl[i+1];
				snprintf(s, 64, "%s (%.04f) +%.04f (oversize)",
				    dsNear->name, dsNear->v,
				    fabs(dsNear->v - in));
				break;
			}
		}
	} else {
		for (i = 0; i < n; i++) {
			ds = &tbl[i];
			if (fabs(ds->v - in) < 0.001) {
				strlcpy(s, ds->name, 64);
				break;
			}
		}
	}
	return (s);
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

static char *
nearest_fractional(double inches)
{	
	int ths = 2, thsNearest = 999, iNearest = 999, i;
	double errNearest = HUGE_VAL;
	double nearest = HUGE_VAL;
	double d;
	char *s;

	if (inches == 1.0) {
		return strdup("1");
	}
	if ((s = malloc(64)) == NULL) {
		return (NULL);
	}
	s[0] = '\0';
	for (ths = 2; ths <= maxFrac; ths *= 2) {
		for (i = 1; i < ths; i++) {
			d = (double)i/(double)ths;
			if (fabs(inches - d) < errNearest) {
				errNearest = fabs(inches - d);
				nearest = d;
				iNearest = i;
				thsNearest = ths;
			}
		}
	}
	if (errNearest < 0.0001) {
		snprintf(s, 64, "%d/%d (%.04f)", iNearest, thsNearest,
		    nearest);
	} else if (nearest > inches) {
		snprintf(s, 64, "%d/%d (%.04f) -%.04f",
		    iNearest, thsNearest, nearest, errNearest);
	} else {
		snprintf(s, 64, "%d/%d (%.04f) +%.04f",
		    iNearest, thsNearest, nearest, errNearest);
	}
	return (s);
}

static double
parse_inches(const char *in)
{
	char *s, *ep;
	double v;

	if ((s = strchr(in, '/')) != NULL) {
		int num = atoi(in);
		int den = atoi(s+1);

		if (s == '\0' || den == 0)
			errx(1, "-i: invalid fraction");

		v = (double)num/(double)den;
	} else {
		v = strtod(in, &ep);
		if (in[0] == '\0' || *ep != '\0')
			errx(1, "-i: invalid value");
	}
	return (v);
}

int
main(int argc, char *argv[])
{
	char *ep, *s, c, ch;
	const char *err;
	char *letter = NULL, *wireno = NULL;
	int pt, i;

	while ((ch = getopt(argc, argv, "ouRp:i:l:m:w:?h")) != -1) {
		switch (ch) {
		case 'u':
			undersize = 1;
			break;
		case 'o':
			if (undersize) {
				errx(1, "-u and -o are mutually exclusive");
			}
			oversize = 1;
			break;
		case 'R':
			Rflag = 1;
			break;
		case 'p':
			Pflag = 1;
			Pangle = (int)strtonum(optarg, 1, 180, &err);
			if (err != NULL) { errx(1, "-p: %s", err); }
			break;
		case 'i':
			inches = parse_inches(optarg);
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
	argc -= optind;
	argv += optind;
	if (argc > 0)
		inches = parse_inches(argv[0]);

	if (wireno) {
		inches = lookup_wireno(wireno);
	} else if (letter) {
		inches = lookup_letter(letter);
	} else {
		if (inches == 0.0)
			printusage();
	}
	printf("Inches:     %.4f\n", inches);
	printf("Fractional: %s\n", nearest_fractional(inches));
	printf("Wire no.:   %s\n", lookuptbl(inches, drills_wireno, ndrills_wireno));
	printf("Letter:     %s\n", lookuptbl(inches, drills_letter, ndrills_letter));
	printf("Metric:     %.02f mm\n", inches*25.4);

	return (0);
}

