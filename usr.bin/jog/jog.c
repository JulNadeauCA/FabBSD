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
#include <cnc.h>

extern char *__progname;

static void
printusage(void)
{
	printf("Usage: %s [-m multiplier] [-S startvel] [-F feedrate] [-A accellim] "
	       "[-J jerklim] [pos ...]\n", __progname);
}

int
main(int argc, char *argv[])
{
	struct cnc_velocity vel;
	int i, ch;
	int mult = 1000;
	
	if (cnc_init() == -1) {
		errx(1, "%s", cnc_get_error());
	}
	atexit(cnc_destroy);
	vel = cnc_vel_default;
	
	while ((ch = getopt(argc, argv, "m:S:F:A:J:?hv")) != -1) {
		switch (ch) {
		case 'm':
			mult = atoi(optarg);
			break;
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
		default:
			printusage();
			return (1);
		}
	}

	if (cncjog(&vel, mult) != 0) {
		errx(1, "cncjog failed");
	}
	return (0);
}

