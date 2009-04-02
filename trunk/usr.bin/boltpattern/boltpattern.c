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

#include <cnc.h>
#include <stdio.h>
#include <unistd.h>

cnc_vec_t C;
cnc_dist_t radius;
cnc_dist_t depth;
int numHoles = 0;
int tool = -1;

static void
printusage(void)
{
	extern char *__progname;
	printf("Usage: %s [-c center] [-r radius] [-n num-holes] [-d depth] "
	       "[-t tool]\n", __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	int ch, i, centerOk = 0, radiusOk = 0, depthOk = 0;

	cnc_vec_zero(&C);
	radius = 0;
	depth = 0;

	while ((ch = getopt(argc, argv, "c:r:n:d:t:?h")) != -1) {
		switch (ch) {
		case 'c':
			cnc_vec_parse(&C, optarg);
			break;
		case 'r':
			cnc_dist_parse(&radius, optarg);
			break;
		case 'n':
			numHoles = atoi(optarg);
			break;
		case 'd':
			cnc_dist_parse(&depth, optarg);
			break;
		case 't':
			tool = atoi(optarg);
			break;
		default:
			printusage();
		}
	}
	if (radius == 0 || depth == 0)
		errx(1, "radius or depth not specified");

	/* ... */

	return (0);
fail:
	fprintf(stderr, "%s\n", CNC_GetError());
	return (1);
}

