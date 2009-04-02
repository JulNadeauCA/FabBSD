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

#include <sys/types.h>
#include <sys/cnc.h>
#include <stdio.h>
#include <unistd.h>

extern char *__progname;

static void
printusage(void)
{
	printf("Usage: %s [-s startvel] [-f feedrate] [-a accellim] "
	       "[-j jerklim] [position ...]\n", __progname);
}

int
main(int argc, char *argv[])
{
	struct cnc_velocity Vp;
	int i, ch;

	Vp.v0 = 0;
	Vp.F = 100;
	Vp.Amax = 10;
	Vp.Jmax = 10;

	while ((ch = getopt(argc, argv, "s:f:a:j:?h")) != -1) {
		switch (ch) {
		case 's':
			cnc_parse_velocity(&Vp.v0, optarg);
			break;
		case 'F':
			cnc_parse_velocity(&Vp.F, optarg);
			break;
		case 'A':
			cnc_parse_velocity(&Vp.Amax, optarg);
			break;
		case 'J':
			cnc_parse_velocity(&Vp.Jmax, optarg);
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
		if (cnc_vec_parse(&v, argv[i]) == -1 ||
		    cncmove(&Vp, &v) == -1) {
			fprintf(stderr, "%s: %s\n", __progname, cnc_get_error());
			return (1);
		}
	}
	return (0);
}

