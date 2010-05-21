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
#include <sys/cnc.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <cnc.h>

extern char *__progname;

char mpg_device[FILENAME_MAX];
int verbose = 1;
int simulate = 0;
struct cnc_velocity vel;
struct cnc_vector pos;

static void
printusage(void)
{
	printf("Usage: %s [-nq] [-t seconds] [-d mpgdevice] [-M multiplier] "
	       "[-S startvel] [-F feedrate] [-A accellim] "
	       "[-J jerklim]\n", __progname);
}

static void
process_timeout(int signo)
{
	int save_errno = errno;
	
	if (!simulate &&
	    cncmove(&vel, &pos) != 0)
		err(1, "cncmove");

	errno = save_errno;
}

int
main(int argc, char *argv[])
{
	char pb[128];
	struct cnc_mpg_event me;
	int i, ch, mpgfd = -1, cncfd;
	int mult = 4000;
	struct itimerval timeout;
	ssize_t rv;

	if (cnc_init() == -1) {
		errx(1, "%s", cnc_get_error());
	}
	atexit(cnc_destroy);
	vel = cnc_vel_default;
	mpg_device[0] = '\0';
	memset(&timeout, 0, sizeof(timeout));
	timeout.it_value.tv_sec = 0;
	timeout.it_value.tv_usec = 500000;
	
	while ((ch = getopt(argc, argv, "nqt:d:M:S:F:A:J:?hv")) != -1) {
		switch (ch) {
		case 'n':
			simulate = 1;
			break;
		case 'q':
			verbose = 0;
			break;
		case 't':
			timeout.it_value.tv_sec = (long)atoi(optarg);
			break;
		case 'd':
			strlcpy(mpg_device, optarg, sizeof(mpg_device));
			break;
		case 'M':
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

	/* Retrieve the current machine position. */
	if ((cncfd = open("/dev/cnc", O_RDONLY)) == -1) {
		err(1, "/dev/cnc");
	}
	if (ioctl(cncfd, CNC_GETPOS, &pos) == -1) {
		errx(1, "CNC_GETPOS");
	}
	if (verbose) {
		cnc_vec_print(&pos, pb, sizeof(pb));
		printf("Starting at %s\n", pb);
	}
	close(cncfd);

	/* Open the MPG device for reading. */
	if (mpg_device[0] != '\0') {
		mpgfd = open(mpg_device, O_RDONLY);
	} else {
		for (i = 0; i < 10; i++) {
			snprintf(mpg_device, sizeof(mpg_device), "/dev/mpg%d", i);
			if ((mpgfd = open(mpg_device, O_RDONLY)) != -1)
				break;
		}
	}
	if (mpgfd == -1)
		err(1, mpg_device);

	signal(SIGALRM, process_timeout);
	
	/* Loop reading MPG events. */
	for (;;) {
		rv = read(mpgfd, &me, sizeof(struct cnc_mpg_event));
		if (rv == -1) {
			if (errno == EINTR) {
				continue;
			}
			err(1, "read");
		} else if (rv < sizeof(struct cnc_mpg_event)) {
			break;
		}
		if (me.delta == 0) {
			continue;
		}
		if (me.axis < 0 || me.axis > CNC_MAX_AXES) {
			errx(1, "invalid axis");
		}
		if (me.delta < 0) {
			pos.v[me.axis] -= mult;
		} else {
			pos.v[me.axis] += mult;
		}
		if (verbose) {
			cnc_vec_print(&pos, pb, sizeof(pb));
			printf("Moving axis%d (target %s)\n", me.axis, pb);
		}
		setitimer(ITIMER_REAL, &timeout, NULL);
	}

	close(mpgfd);
	return (0);
}

