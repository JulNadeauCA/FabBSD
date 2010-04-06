/*
 * Copyright (c) 2009-2010 Hypertriton, Inc. <http://hypertriton.com/>
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

#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <cnc.h>

extern char *__progname;
int verbose = 0;

static void
printusage(void)
{
	printf("Usage: %s [-v] [-S startvel] [-F feedrate] [-A accellim] "
	       "[-J jerklim] [pos ...]\n", __progname);
}

int
main(int argc, char *argv[])
{
	struct cnc_velocity vel;
	int i, ch;

	if (cnc_init() == -1) {
		errx(1, "%s", cnc_get_error());
	}
	atexit(cnc_destroy);
	vel = cnc_vel_default;

	while ((ch = getopt(argc, argv, "S:F:A:J:?hv")) != -1) {
		switch (ch) {
		case 'S':
			cnc_vel_parse(&vel.v0, optarg);
			break;
		case 'F':
			cnc_vel_parse(&vel.F, optarg);
			break;
		case 'A':
			cnc_vel_parse(&vel.Amax, optarg);
			break;
		case 'J':
			cnc_vel_parse(&vel.Jmax, optarg);
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			printusage();
			return (1);
		}
	}
	argv += optind;
	argc -= optind;
	for (i = 0; i < argc; i++) {
		cnc_vec_t v;

		if (cnc_vec_parse(&v, argv[i]) == -1)
			errx(1, "parsing position: %s", cnc_get_error());
		if (verbose) {
			char vs[32];
			cnc_vec_print(&v, vs, sizeof(vs));
			printf("moving to %s...", vs);
		}
		if (cncmove(&vel, &v) != 0) {
			errx(1, "cncmove failed");
		}
		if (verbose)
			printf("\n");
	}
	return (0);
}

