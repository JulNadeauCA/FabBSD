/*	$FabBSD$	*/
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

/*
 * Program to control the cnc(4) interface.
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/limits.h>

#include <limits.h>
#include <cnc.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pathnames.h"

static const char *showopt_list[] = {
	"devices", "info", "limits", "position", "tools", NULL
};
static const char *flushopt_list[] = {
	"info", NULL
};

const char *device = _PATH_DEV_CNC;
const char *timingsfile = _PATH_TIMINGS;
const char *showopt = NULL;
const char *flushopt = NULL;
int devfd = -1;
int quiet = 0;

static void
printusage(const char *optlist[])
{
	extern char *__progname;

	if (optlist != NULL) {
		const char **s = &optlist[0];
		fprintf(stderr, "acceptable modifiers: ");
		while (*s != NULL) {
			fputs(*s, stderr);
			fputc(' ', stderr);
			s++;
		}
		fputc('\n', stderr);
	} else {
		fprintf(stderr, "usage: %s [-qCc] [-c timingfile] "
		                "[-s modifier] [-F modifier] [-p coords]"
				"\n", __progname);
	}
	exit(1);
}

static const char *
lookup_option(char *cmd, const char **list)
{
	if (cmd != NULL && *cmd) {
		for (; *list; list++)
			if (!strncmp(cmd, *list, strlen(cmd)))
				return (*list);
	}
	return (NULL);
}

static int
calibrate_timings(void)
{
	struct cnc_timings t;
	FILE *f;

	t.hz = 0;
	
	if ((f = fopen(_PATH_TIMINGS, "r")) != NULL) {
		char buf[128], *s = &buf[0];
		char *s1, *s2;

		fread(buf, sizeof(buf), 1, f);
		s1 = strsep(&s, "\n");
		s2 = strsep(&s, "\n");
		t.hz = (cnc_utime_t)strtoull(s1, (char **)NULL, 10);
		fclose(f);
	}
	if (ioctl(devfd, CNC_SETTIMINGS, &t) == -1) {
		err(1, "CNC_SETTIMINGS");
	}
	if (t.hz == 0) {
		printf("Calibrating timings. This may take several minutes...\n");
		if (ioctl(devfd, CNC_CALTIMINGS, &t) == -1) {
			err(1, "CNC_CALTIMINGS");
		}
		printf("Saving timings to %s\n", _PATH_TIMINGS);
		if ((f = fopen(_PATH_TIMINGS, "w")) != NULL) {
			fprintf(f, "%llu\n", t.hz);
			fclose(f);
		} else {
			warn("%s", _PATH_TIMINGS);
		}
	}
	if (!quiet) {
		printf("Timings calibrated successfully (1Hz=%llu)\n", t.hz);
	}
	return ioctl(devfd, CNC_SETTIMINGS, &t);
}

int
main(int argc, char *argv[])
{
	struct cnc_vector pos;
	char pb[128];
	int ch;
	int do_cal = 0;
	
	if ((devfd = open(device, O_WRONLY)) == -1)
		err(1, "%s", device);

	while ((ch = getopt(argc, argv, "Cs:F:p:")) != -1) {
		switch (ch) {
		case 'C':
			do_cal = 1;
			break;
		case 's':
			if ((showopt = lookup_option(optarg, showopt_list))
			    == NULL) {
				warnx("Unknown show modifier '%s'", optarg);
				printusage(showopt_list);
			}
			break;
		case 'F':
			if ((flushopt = lookup_option(optarg, flushopt_list))
			    == NULL) {
				warnx("Unknown flush modifier '%s'", optarg);
				printusage(flushopt_list);
			}
			break;
		case 'q':
			quiet = 1;
			break;
		case 'p':
			if (!quiet) {
				if (ioctl(devfd, CNC_GETPOS, &pos) == -1) {
					errx(1, "CNC_GETPOS");
				}
				cnc_vec_print(&pos, pb, sizeof(pb));
				printf("%s -> %s\n", pb, optarg);
			}
			if (cnc_vec_parse(&pos, optarg) == -1)
				errx(1, "-p: %s", cnc_get_error());
			if (ioctl(devfd, CNC_SETPOS, &pos) == -1)
				errx(1, "CNC_SETPOS");
			break;
		default:
			printusage(NULL);
		}
	}
	if (argc == 0)
		printusage(NULL);
	
	if (do_cal) {
		calibrate_timings();
	} else if (showopt) {
		switch (*showopt) {
		case 'p':
			{
				struct cnc_vector pos;
				char pb[128];

				if (ioctl(devfd, CNC_GETPOS, &pos) == -1) {
					errx(1, "CNC_GETPOS");
				}
				cnc_vec_print(&pos, pb, sizeof(pb));
				fputs(pb, stdout);
				fputc('\n', stdout);
			}
			break;
		case 'd':
			{
				struct cnc_device_info di;

				if (ioctl(devfd, CNC_GETDEVICEINFO, &di) == -1) {
					errx(1, "CNC_GETDEVICEINFO");
				}
				printf("Devices:\n");
				printf("  %d servos\n", di.nservos);
				printf("  %d spindles\n", di.nspindles);
				printf("  %d estops\n", di.nestops);
				printf("  %d encoders\n", di.nencoders);
				printf("  %d MPGs\n", di.nmpgs);
			}
			break;
		case 'i':
			{
				struct cnc_stats st;

				if (ioctl(devfd, CNC_GETSTATS, &st) == -1) {
					errx(1, "CNC_GETSTATS");
				}
				printf("Counters\n");
				printf("  peak-velocity %lu\n",	(u_long)st.peak_velocity);
				printf("  e-stops       %lu\n",	(u_long)st.estops);
			}
			break;
		case 'l':
			{
				struct cnc_kinlimits kl;
				if (ioctl(devfd, CNC_GETKINLIMITS, &kl) == -1) {
					errx(1, "CNC_GETKINLIMITS");
				}
				printf("Kinematic limits\n");
				printf("  max-velocity %lu\n", (u_long)kl.Fmax);
				printf("  max-accel    %lu\n", (u_long)kl.Amax);
				printf("  max-jerk     %lu\n", (u_long)kl.Jmax);
			}
			break;
		case 't':
			{

				struct cnc_tool t;
				int i, ntools;

				if (ioctl(devfd, CNC_GETNTOOLS, &ntools)
				    == -1) {
					errx(1, "CNC_GETNTOOLS");
				}
				printf("%d tools:\n", ntools);
				for (i = 0; i < ntools; i++) {
					t.idx = i;
					if (ioctl(devfd, CNC_GETTOOLINFO, &t)
					    == -1) {
						errx(1, "CNC_GETTOOLINFO(%d)", t.idx);
					}
					cnc_tool_print(&t);
				}
			}
			break;
		}
	} else if (flushopt) {
		switch (*showopt) {
		case 'i':
			if (ioctl(devfd, CNC_CLRSTATS) == -1)
				errx(1, "CNC_CLRSTATS");
			break;
		}
	}

	close(devfd);
	return (0);
}
