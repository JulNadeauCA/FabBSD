/*
 * Copyright (c) 2011 Hypertriton, Inc. <http://hypertriton.com/>
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

int maxDiv = 64;
int conv_metric = 0;

static void
printusage(void)
{
	extern char *__progname;
	printf("Usage: %s [-mi] [-M maxdiv] [number]\n", __progname);
	exit(1);
}

static void
print_nearest_fractional(double val)
{	
	int ths = 2, thsNearest = 999, iNearest = 999, i;
	double errNearest = HUGE_VAL;
	double nearest = HUGE_VAL;
	double d;

	if (fabs(val - 1.0) < 0.00005) {
		printf("1\n");
	}
	for (ths = 2; ths <= maxDiv; ths *= 2) {
		for (i = 1; i < ths; i++) {
			d = (double)i/(double)ths;
			if (fabs(val - d) < errNearest) {
				errNearest = fabs(val - d);
				nearest = d;
				iNearest = i;
				thsNearest = ths;
			}
		}
	}
	if (errNearest < 0.00005) {
		printf("%d/%d (%.04f)\n", iNearest, thsNearest, nearest);
	} else if (nearest > val) {
		printf("%d/%d (%.04f) -%.04f\n",
		    iNearest, thsNearest, nearest, errNearest);
	} else {
		printf("%d/%d (%.04f) +%.04f\n",
		    iNearest, thsNearest, nearest, errNearest);
	}
}

int
main(int argc, char *argv[])
{
	char *ep, *s, c, ch;
	int pt, i;

	while ((ch = getopt(argc, argv, "M:m?h")) != -1) {
		switch (ch) {
		case 'M':
			maxDiv = atoi(optarg);
			break;
		case 'm':
			conv_metric = 1;
			break;
		default:
			printusage();
		}
	}
	argv += optind;
	argc -= optind;
	if (argc == 0)
		printusage();

	for (i = 0; i < argc; i++) {
		double val;
		char *arg = argv[i];

		if ((s = strchr(arg, '/')) != NULL) {
			int num = atoi(arg);
			int den = atoi(s+1);

			if (s == '\0' || den == 0) {
				errx(1, "-i: invalid fraction");
			}
			val = (double)num/den;
		} else {
			val = strtod(arg, &ep);
			if (arg[0] == '\0' || *ep != '\0') { errx(1, "-i: invalid value"); }
		}
		if (conv_metric) {
			val /= 25.4;
		}
		print_nearest_fractional(val);
		
	}
	return (0);
}

