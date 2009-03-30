/*	$OpenBSD: kgmon.c,v 1.14 2007/11/26 09:28:34 martynas Exp $	*/

/*
 * Copyright (c) 1983, 1992, 1993
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
static char copyright[] =
"@(#) Copyright (c) 1983, 1992, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
/*static char sccsid[] = "from: @(#)kgmon.c	8.1 (Berkeley) 6/6/93";*/
static char *rcsid = "$OpenBSD: kgmon.c,v 1.14 2007/11/26 09:28:34 martynas Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/file.h>
#include <sys/sysctl.h>
#include <sys/gmon.h>
#include <errno.h>
#include <err.h>
#include <kvm.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <nlist.h>
#include <ctype.h>
#include <paths.h>

struct nlist nl[] = {
#define	N_GMONPARAM	0
	{ "__gmonparam" },
#define	N_PROFHZ	1
	{ "_profhz" },
	{ NULL }
};

struct kvmvars {
	kvm_t	*kd;
	struct gmonparam gpm;
};

int	bflag, hflag, kflag, rflag, pflag;
int	debug = 0;
void	setprof(struct kvmvars *, int);
void	dumpstate(struct kvmvars *);
void	reset(struct kvmvars *);
void	kern_readonly(int);
int	getprof(struct kvmvars *);
int	getprofhz(struct kvmvars *);
int	openfiles(char *, char *, struct kvmvars *);

int
main(int argc, char **argv)
{
	extern char *__progname;
	int ch, mode, disp, accessmode;
	struct kvmvars kvmvars;
	char *sys, *kmemf;

	seteuid(getuid());
	kmemf = NULL;
	sys = NULL;
	while ((ch = getopt(argc, argv, "M:N:bhpr")) != -1) {
		switch((char)ch) {

		case 'M':
			kmemf = optarg;
			kflag = 1;
			break;

		case 'N':
			sys = optarg;
			break;

		case 'b':
			bflag = 1;
			break;

		case 'h':
			hflag = 1;
			break;

		case 'p':
			pflag = 1;
			break;

		case 'r':
			rflag = 1;
			break;

		default:
			fprintf(stderr,
			    "usage: %s [-bhpr] [-M core] [-N system]\n",
			    __progname);
			exit(1);
		}
	}
	argc -= optind;
	argv += optind;

#define BACKWARD_COMPATIBILITY
#ifdef	BACKWARD_COMPATIBILITY
	if (*argv) {
		sys = *argv;
		if (*++argv) {
			kmemf = *argv;
			++kflag;
		}
	}
#endif
	accessmode = openfiles(sys, kmemf, &kvmvars);
	mode = getprof(&kvmvars);
	if (hflag)
		disp = GMON_PROF_OFF;
	else if (bflag)
		disp = GMON_PROF_ON;
	else
		disp = mode;
	if (pflag)
		dumpstate(&kvmvars);
	if (rflag)
		reset(&kvmvars);
	if (accessmode == O_RDWR)
		setprof(&kvmvars, disp);
	printf("%s: kernel profiling is %s.\n", __progname,
	    disp == GMON_PROF_OFF ? "off" : "running");
	return (0);
}

/*
 * Check that profiling is enabled and open any ncessary files.
 */
int
openfiles(char *sys, char *kmemf, struct kvmvars *kvp)
{
	int mib[3], state, openmode;
	size_t size;
	char errbuf[_POSIX2_LINE_MAX];

	if (!kflag) {
		mib[0] = CTL_KERN;
		mib[1] = KERN_PROF;
		mib[2] = GPROF_STATE;
		size = sizeof state;
		if (sysctl(mib, 3, &state, &size, NULL, 0) < 0)
			errx(20, "profiling not defined in kernel.");
		if (!(bflag || hflag || rflag ||
		    (pflag && state == GMON_PROF_ON)))
			return (O_RDONLY);
		(void)seteuid(0);
		if (sysctl(mib, 3, NULL, NULL, &state, size) >= 0)
			return (O_RDWR);
		(void)seteuid(getuid());
		kern_readonly(state);
		return (O_RDONLY);
	}
	openmode = (bflag || hflag || pflag || rflag) ? O_RDWR : O_RDONLY;
	kvp->kd = kvm_openfiles(sys, kmemf, NULL, openmode, errbuf);
	if (kvp->kd == NULL) {
		if (openmode == O_RDWR) {
			openmode = O_RDONLY;
			kvp->kd = kvm_openfiles(sys, kmemf, NULL, O_RDONLY,
			    errbuf);
		}
		if (kvp->kd == NULL)
			errx(2, "kvm_openfiles: %s", errbuf);
		kern_readonly(GMON_PROF_ON);
	}
	if (kvm_nlist(kvp->kd, nl) < 0)
		errx(3, "%s: no namelist", sys ? sys : _PATH_UNIX);
	if (!nl[N_GMONPARAM].n_value)
		errx(20, "profiling not defined in kernel.");
	return (openmode);
}

/*
 * Suppress options that require a writable kernel.
 */
void
kern_readonly(int mode)
{
	extern char *__progname;

	(void)fprintf(stderr, "%s: kernel read-only: ", __progname);
	if (pflag && mode == GMON_PROF_ON)
		(void)fprintf(stderr, "data may be inconsistent\n");
	if (rflag)
		(void)fprintf(stderr, "-r suppressed\n");
	if (bflag)
		(void)fprintf(stderr, "-b suppressed\n");
	if (hflag)
		(void)fprintf(stderr, "-h suppressed\n");
	rflag = bflag = hflag = 0;
}

/*
 * Get the state of kernel profiling.
 */
int
getprof(struct kvmvars *kvp)
{
	int mib[3];
	size_t size;

	if (kflag) {
		size = kvm_read(kvp->kd, nl[N_GMONPARAM].n_value, &kvp->gpm,
		    sizeof kvp->gpm);
	} else {
		mib[0] = CTL_KERN;
		mib[1] = KERN_PROF;
		mib[2] = GPROF_GMONPARAM;
		size = sizeof kvp->gpm;
		if (sysctl(mib, 3, &kvp->gpm, &size, NULL, 0) < 0)
			size = 0;
	}
	if (size != sizeof kvp->gpm)
		errx(4, "cannot get gmonparam: %s",
		    kflag ? kvm_geterr(kvp->kd) : strerror(errno));
	return (kvp->gpm.state);
}

/*
 * Enable or disable kernel profiling according to the state variable.
 */
void
setprof(struct kvmvars *kvp, int state)
{
	struct gmonparam *p = (struct gmonparam *)nl[N_GMONPARAM].n_value;
	int mib[3], oldstate;
	size_t sz;

	sz = sizeof(state);
	if (!kflag) {
		mib[0] = CTL_KERN;
		mib[1] = KERN_PROF;
		mib[2] = GPROF_STATE;
		if (sysctl(mib, 3, &oldstate, &sz, NULL, 0) < 0)
			goto bad;
		if (oldstate == state)
			return;
		(void)seteuid(0);
		if (sysctl(mib, 3, NULL, NULL, &state, sz) >= 0) {
			(void)seteuid(getuid());
			return;
		}
		(void)seteuid(getuid());
	} else if (kvm_write(kvp->kd, (u_long)&p->state, (void *)&state, sz)
	    == sz)
		return;
bad:
	warnx("warning: cannot turn profiling %s",
	    state == GMON_PROF_OFF ? "off" : "on");
}

/*
 * Build the gmon.out file.
 */
void
dumpstate(struct kvmvars *kvp)
{
	FILE *fp;
	struct rawarc rawarc;
	struct tostruct *tos;
	u_long frompc;
	u_short *froms, *tickbuf;
	int mib[3];
	size_t i;
	struct gmonhdr h;
	int fromindex, endfrom, toindex;

	setprof(kvp, GMON_PROF_OFF);
	fp = fopen("gmon.out", "w");
	if (fp == 0) {
		perror("gmon.out");
		return;
	}

	/*
	 * Build the gmon header and write it to a file.
	 */
	bzero(&h, sizeof(h));
	h.lpc = kvp->gpm.lowpc;
	h.hpc = kvp->gpm.highpc;
	h.ncnt = kvp->gpm.kcountsize + sizeof(h);
	h.version = GMONVERSION;
	h.profrate = getprofhz(kvp);
	fwrite((char *)&h, sizeof(h), 1, fp);

	/*
	 * Write out the tick buffer.
	 */
	mib[0] = CTL_KERN;
	mib[1] = KERN_PROF;
	if ((tickbuf = (u_short *)malloc(kvp->gpm.kcountsize)) == NULL)
		errx(5, "cannot allocate kcount space");
	if (kflag) {
		i = kvm_read(kvp->kd, (u_long)kvp->gpm.kcount, (void *)tickbuf,
		    kvp->gpm.kcountsize);
	} else {
		mib[2] = GPROF_COUNT;
		i = kvp->gpm.kcountsize;
		if (sysctl(mib, 3, tickbuf, &i, NULL, 0) < 0)
			i = 0;
	}
	if (i != kvp->gpm.kcountsize)
		errx(6, "read ticks: read %lu, got %zu: %s",
		    kvp->gpm.kcountsize, i,
		    kflag ? kvm_geterr(kvp->kd) : strerror(errno));
	if ((fwrite(tickbuf, kvp->gpm.kcountsize, 1, fp)) != 1)
		err(7, "writing tocks to gmon.out");
	free(tickbuf);

	/*
	 * Write out the arc info.
	 */
	if ((froms = (u_short *)malloc(kvp->gpm.fromssize)) == NULL)
		errx(8, "cannot allocate froms space");
	if (kflag) {
		i = kvm_read(kvp->kd, (u_long)kvp->gpm.froms, (void *)froms,
		    kvp->gpm.fromssize);
	} else {
		mib[2] = GPROF_FROMS;
		i = kvp->gpm.fromssize;
		if (sysctl(mib, 3, froms, &i, NULL, 0) < 0)
			i = 0;
	}
	if (i != kvp->gpm.fromssize)
		errx(9, "read froms: read %lu, got %zu: %s",
		    kvp->gpm.fromssize, i,
		    kflag ? kvm_geterr(kvp->kd) : strerror(errno));
	if ((tos = (struct tostruct *)malloc(kvp->gpm.tossize)) == NULL)
		errx(10, "cannot allocate tos space");
	if (kflag) {
		i = kvm_read(kvp->kd, (u_long)kvp->gpm.tos, (void *)tos,
		    kvp->gpm.tossize);
	} else {
		mib[2] = GPROF_TOS;
		i = kvp->gpm.tossize;
		if (sysctl(mib, 3, tos, &i, NULL, 0) < 0)
			i = 0;
	}
	if (i != kvp->gpm.tossize)
		errx(11, "read tos: read %lu, got %zu: %s",
		    kvp->gpm.tossize, i,
		    kflag ? kvm_geterr(kvp->kd) : strerror(errno));
	if (debug)
		warnx("lowpc 0x%lx, textsize 0x%lx",
		    kvp->gpm.lowpc, kvp->gpm.textsize);
	endfrom = kvp->gpm.fromssize / sizeof(*froms);
	for (fromindex = 0; fromindex < endfrom; ++fromindex) {
		if (froms[fromindex] == 0)
			continue;
		frompc = (u_long)kvp->gpm.lowpc +
		    (fromindex * kvp->gpm.hashfraction * sizeof(*froms));
		for (toindex = froms[fromindex]; toindex != 0;
		   toindex = tos[toindex].link) {
			if (debug)
			  warnx("[mcleanup] frompc 0x%lx selfpc 0x%lx count %ld",
			    frompc, tos[toindex].selfpc, tos[toindex].count);
			rawarc.raw_frompc = frompc;
			rawarc.raw_selfpc = (u_long)tos[toindex].selfpc;
			rawarc.raw_count = tos[toindex].count;
			fwrite((char *)&rawarc, sizeof(rawarc), 1, fp);
		}
	}
	fclose(fp);
}

/*
 * Get the profiling rate.
 */
int
getprofhz(struct kvmvars *kvp)
{
	int mib[2], profrate;
	size_t size;
	struct clockinfo clockrate;

	if (kflag) {
		profrate = 1;
		if (kvm_read(kvp->kd, nl[N_PROFHZ].n_value, &profrate,
		    sizeof profrate) != sizeof profrate)
			warnx("get clockrate: %s", kvm_geterr(kvp->kd));
		return (profrate);
	}
	mib[0] = CTL_KERN;
	mib[1] = KERN_CLOCKRATE;
	clockrate.profhz = 1;
	size = sizeof clockrate;
	if (sysctl(mib, 2, &clockrate, &size, NULL, 0) < 0)
		warn("get clockrate");
	return (clockrate.profhz);
}

/*
 * Reset the kernel profiling date structures.
 */
void
reset(struct kvmvars *kvp)
{
	char *zbuf;
	u_long biggest;
	int mib[3];

	setprof(kvp, GMON_PROF_OFF);

	biggest = kvp->gpm.kcountsize;
	if (kvp->gpm.fromssize > biggest)
		biggest = kvp->gpm.fromssize;
	if (kvp->gpm.tossize > biggest)
		biggest = kvp->gpm.tossize;
	if ((zbuf = (char *)malloc(biggest)) == NULL)
		errx(12, "cannot allocate zbuf space");
	bzero(zbuf, biggest);
	if (kflag) {
		if (kvm_write(kvp->kd, (u_long)kvp->gpm.kcount, zbuf,
		    kvp->gpm.kcountsize) != kvp->gpm.kcountsize)
			errx(13, "tickbuf zero: %s", kvm_geterr(kvp->kd));
		if (kvm_write(kvp->kd, (u_long)kvp->gpm.froms, zbuf,
		    kvp->gpm.fromssize) != kvp->gpm.fromssize)
			errx(14, "froms zero: %s", kvm_geterr(kvp->kd));
		if (kvm_write(kvp->kd, (u_long)kvp->gpm.tos, zbuf,
		    kvp->gpm.tossize) != kvp->gpm.tossize)
			errx(15, "tos zero: %s", kvm_geterr(kvp->kd));
		return;
	}
	(void)seteuid(0);
	mib[0] = CTL_KERN;
	mib[1] = KERN_PROF;
	mib[2] = GPROF_COUNT;
	if (sysctl(mib, 3, NULL, NULL, zbuf, kvp->gpm.kcountsize) < 0)
		err(13, "tickbuf zero");
	mib[2] = GPROF_FROMS;
	if (sysctl(mib, 3, NULL, NULL, zbuf, kvp->gpm.fromssize) < 0)
		err(14, "froms zero");
	mib[2] = GPROF_TOS;
	if (sysctl(mib, 3, NULL, NULL, zbuf, kvp->gpm.tossize) < 0)
		err(15, "tos zero");
	(void)seteuid(getuid());
	free(zbuf);
}
