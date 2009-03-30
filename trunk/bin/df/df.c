/*	$OpenBSD: df.c,v 1.49 2008/03/16 20:04:35 otto Exp $	*/
/*	$NetBSD: df.c,v 1.21.2.1 1995/11/01 00:06:11 jtc Exp $	*/

/*
 * Copyright (c) 1980, 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
"@(#) Copyright (c) 1980, 1990, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)df.c	8.7 (Berkeley) 4/2/94";
#else
static char rcsid[] = "$OpenBSD: df.c,v 1.49 2008/03/16 20:04:35 otto Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/mount.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

extern	char *__progname;

char	*getmntpt(char *);
int	 selected(const char *);
void	 maketypelist(char *);
long	 regetmntinfo(struct statfs **, long);
void	 bsdprint(struct statfs *, long, int);
void	 prtstat(struct statfs *, int, int, int);
void	 posixprint(struct statfs *, long, int);
int 	 bread(int, off_t, void *, int);
void	 usage(void);
void	 prthumanval(long long);
void	 prthuman(struct statfs *sfsp, unsigned long long);

int		raw_df(char *, struct statfs *);
extern int	ffs_df(int, char *, struct statfs *);
extern int	e2fs_df(int, char *, struct statfs *);

int	hflag, iflag, kflag, lflag, nflag, Pflag;
char	**typelist = NULL;

int
main(int argc, char *argv[])
{
	struct stat stbuf;
	struct statfs *mntbuf;
	long mntsize;
	int ch, i;
	int width, maxwidth;
	char *mntpt;

	while ((ch = getopt(argc, argv, "hiklnPt:")) != -1)
		switch (ch) {
		case 'h':
			hflag = 1;
			kflag = 0;
			break;
		case 'i':
			iflag = 1;
			break;
		case 'k':
			kflag = 1;
			hflag = 0;
			break;
		case 'l':
			lflag = 1;
			break;
		case 'n':
			nflag = 1;
			break;
		case 'P':
			Pflag = 1;
			break;
		case 't':
			if (typelist != NULL)
				errx(1, "only one -t option may be specified.");
			maketypelist(optarg);
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if ((iflag || hflag) && Pflag) {
		warnx("-h and -i are incompatible with -P");
		usage();
	}

	mntsize = getmntinfo(&mntbuf, MNT_NOWAIT);
	if (mntsize == 0)
		err(1, "retrieving information on mounted file systems");

	if (!*argv) {
		mntsize = regetmntinfo(&mntbuf, mntsize);
	} else {
		mntbuf = calloc(argc, sizeof(struct statfs));
		if (mntbuf == NULL)
			err(1, NULL);
		mntsize = 0;
		for (; *argv; argv++) {
			if (stat(*argv, &stbuf) < 0) {
				if ((mntpt = getmntpt(*argv)) == 0) {
					warn("%s", *argv);
					continue;
				}
			} else if (S_ISCHR(stbuf.st_mode) || S_ISBLK(stbuf.st_mode)) {
				if (!raw_df(*argv, &mntbuf[mntsize]))
					++mntsize;
				continue;
			} else
				mntpt = *argv;
			/*
			 * Statfs does not take a `wait' flag, so we cannot
			 * implement nflag here.
			 */
			if (!statfs(mntpt, &mntbuf[mntsize]))
				if (lflag && (mntbuf[mntsize].f_flags & MNT_LOCAL) == 0)
					warnx("%s is not a local file system",
					    *argv);
				else if (!selected(mntbuf[mntsize].f_fstypename))
					warnx("%s mounted as a %s file system",
					    *argv, mntbuf[mntsize].f_fstypename);
				else
					++mntsize;
			else
				warn("%s", *argv);
		}
	}

	if (mntsize) {
		maxwidth = 0;
		for (i = 0; i < mntsize; i++) {
			width = strlen(mntbuf[i].f_mntfromname);
			if (width > maxwidth)
				maxwidth = width;
		}

		if (maxwidth < 11)
			maxwidth = 11;

		if (Pflag)
			posixprint(mntbuf, mntsize, maxwidth);
		else
			bsdprint(mntbuf, mntsize, maxwidth);
	}

	exit(mntsize ? 0 : 1);
}

char *
getmntpt(char *name)
{
	long mntsize, i;
	struct statfs *mntbuf;

	mntsize = getmntinfo(&mntbuf, MNT_NOWAIT);
	for (i = 0; i < mntsize; i++) {
		if (!strcmp(mntbuf[i].f_mntfromname, name))
			return (mntbuf[i].f_mntonname);
	}
	return (0);
}

static enum { IN_LIST, NOT_IN_LIST } which;

int
selected(const char *type)
{
	char **av;

	/* If no type specified, it's always selected. */
	if (typelist == NULL)
		return (1);
	for (av = typelist; *av != NULL; ++av)
		if (!strncmp(type, *av, MFSNAMELEN))
			return (which == IN_LIST ? 1 : 0);
	return (which == IN_LIST ? 0 : 1);
}

void
maketypelist(char *fslist)
{
	int i;
	char *nextcp, **av;

	if ((fslist == NULL) || (fslist[0] == '\0'))
		errx(1, "empty type list");

	/*
	 * XXX
	 * Note: the syntax is "noxxx,yyy" for no xxx's and
	 * no yyy's, not the more intuitive "noyyy,noyyy".
	 */
	if (fslist[0] == 'n' && fslist[1] == 'o') {
		fslist += 2;
		which = NOT_IN_LIST;
	} else
		which = IN_LIST;

	/* Count the number of types. */
	for (i = 1, nextcp = fslist; (nextcp = strchr(nextcp, ',')) != NULL; i++)
		++nextcp;

	/* Build an array of that many types. */
	if ((av = typelist = calloc(i + 1, sizeof(char *))) == NULL)
		err(1, NULL);
	av[0] = fslist;
	for (i = 1, nextcp = fslist; (nextcp = strchr(nextcp, ',')) != NULL; i++) {
		*nextcp = '\0';
		av[i] = ++nextcp;
	}
	/* Terminate the array. */
	av[i] = NULL;
}

/*
 * Make a pass over the filesystem info in ``mntbuf'' filtering out
 * filesystem types not in ``fsmask'' and possibly re-stating to get
 * current (not cached) info.  Returns the new count of valid statfs bufs.
 */
long
regetmntinfo(struct statfs **mntbufp, long mntsize)
{
	int i, j;
	struct statfs *mntbuf;

	if (!lflag && typelist == NULL)
		return (nflag ? mntsize : getmntinfo(mntbufp, MNT_WAIT));

	mntbuf = *mntbufp;
	j = 0;
	for (i = 0; i < mntsize; i++) {
		if (lflag && (mntbuf[i].f_flags & MNT_LOCAL) == 0)
			continue;
		if (!selected(mntbuf[i].f_fstypename))
			continue;
		if (nflag)
			mntbuf[j] = mntbuf[i];
		else
			(void)statfs(mntbuf[i].f_mntonname, &mntbuf[j]);
		j++;
	}
	return (j);
}

/*
 * "human-readable" output: use 3 digits max.--put unit suffixes at
 * the end.  Makes output compact and easy-to-read esp. on huge disks.
 * Code moved into libutil; this is now just a wrapper.
 */
void
prthumanval(long long bytes)
{
	char ret[FMT_SCALED_STRSIZE];

	if (fmt_scaled(bytes, ret) == -1) {
		(void)printf(" %lld", bytes);
		return;
	}
	(void)printf(" %7s", ret);
}

void
prthuman(struct statfs *sfsp, unsigned long long used)
{
	prthumanval(sfsp->f_blocks * sfsp->f_bsize);
	prthumanval(used * sfsp->f_bsize);
	prthumanval(sfsp->f_bavail * sfsp->f_bsize);
}

/*
 * Convert statfs returned filesystem size into BLOCKSIZE units.
 * Attempts to avoid overflow for large filesystems.
 */
#define fsbtoblk(num, fsbs, bs) \
	(((fsbs) != 0 && (fsbs) < (bs)) ? \
		(num) / ((bs) / (fsbs)) : (num) * ((fsbs) / (bs)))

/*
 * Print out status about a filesystem.
 */
void
prtstat(struct statfs *sfsp, int maxwidth, int headerlen, int blocksize)
{
	u_int64_t used, inodes;
	int64_t availblks;

	(void)printf("%-*.*s", maxwidth, maxwidth, sfsp->f_mntfromname);
	used = sfsp->f_blocks - sfsp->f_bfree;
	availblks = sfsp->f_bavail + used;
	if (hflag)
		prthuman(sfsp, used);
	else
		(void)printf(" %*llu %9llu %9lld", headerlen,
		    fsbtoblk(sfsp->f_blocks, sfsp->f_bsize, blocksize),
		    fsbtoblk(used, sfsp->f_bsize, blocksize),
		    fsbtoblk(sfsp->f_bavail, sfsp->f_bsize, blocksize));
	(void)printf(" %5.0f%%",
	    availblks == 0 ? 100.0 : (double)used / (double)availblks * 100.0);
	if (iflag) {
		inodes = sfsp->f_files;
		used = inodes - sfsp->f_ffree;
		(void)printf(" %7llu %7llu %5.0f%% ", used, sfsp->f_ffree,
		   inodes == 0 ? 100.0 : (double)used / (double)inodes * 100.0);
	} else
		(void)printf("  ");
	(void)printf("  %s\n", sfsp->f_mntonname);
}

/*
 * Print in traditional BSD format.
 */
void
bsdprint(struct statfs *mntbuf, long mntsize, int maxwidth)
{
	int i;
	char *header;
	int headerlen;
	long blocksize;

	/* Print the header line */
	if (hflag) {
		header = "   Size";
		headerlen = strlen(header);
		(void)printf("%-*.*s %s    Used   Avail Capacity",
			     maxwidth, maxwidth, "Filesystem", header);
	} else {
		if (kflag) {
			blocksize = 1024;
			header = "1K-blocks";
			headerlen = strlen(header);
		} else
			header = getbsize(&headerlen, &blocksize);
		(void)printf("%-*.*s %s      Used     Avail Capacity",
			     maxwidth, maxwidth, "Filesystem", header);
	}
	if (iflag)
		(void)printf(" iused   ifree  %%iused");
	(void)printf("  Mounted on\n");


	for (i = 0; i < mntsize; i++)
		prtstat(&mntbuf[i], maxwidth, headerlen, blocksize);
	return;
}

/*
 * Print in format defined by POSIX 1002.2, invoke with -P option.
 */
void
posixprint(struct statfs *mntbuf, long mntsize, int maxwidth)
{
	int i;
	int blocksize;
	char *blockstr;
	struct statfs *sfsp;
	long long used, avail;
	double percentused;

	if (kflag) {
		blocksize = 1024;
		blockstr = "1024-blocks";
	} else {
		blocksize = 512;
		blockstr = " 512-blocks";
	}

	(void)printf(
	    "%-*.*s %s       Used   Available Capacity Mounted on\n",
	    maxwidth, maxwidth, "Filesystem", blockstr);

	for (i = 0; i < mntsize; i++) {
		sfsp = &mntbuf[i];
		used = sfsp->f_blocks - sfsp->f_bfree;
		avail = sfsp->f_bavail + used;
		if (avail == 0)
			percentused = 100.0;
		else
			percentused = (double)used / (double)avail * 100.0;

		(void) printf ("%-*.*s %*lld %10lld %11lld %5.0f%%   %s\n",
			maxwidth, maxwidth, sfsp->f_mntfromname,
			(int)strlen(blockstr),
			fsbtoblk(sfsp->f_blocks, sfsp->f_bsize, blocksize),
			fsbtoblk(used, sfsp->f_bsize, blocksize),
			fsbtoblk(sfsp->f_bavail, sfsp->f_bsize, blocksize),
			percentused, sfsp->f_mntonname);
	}
}

int
raw_df(char *file, struct statfs *sfsp)
{
	int rfd, ret = -1;

	if ((rfd = open(file, O_RDONLY)) < 0) {
		warn("%s", file);
		return (-1);
	}

	if (ffs_df(rfd, file, sfsp) == 0) {
		ret = 0;
	} else if (e2fs_df(rfd, file, sfsp) == 0) {
		ret = 0;
	}

	close (rfd);
	return (ret);
}

int
bread(int rfd, off_t off, void *buf, int cnt)
{
	int nr;

	(void)lseek(rfd, off, SEEK_SET);
	if ((nr = read(rfd, buf, cnt)) != cnt) {
		/* Probably a dismounted disk if errno == EIO. */
		if (errno != EIO)
			(void)fprintf(stderr, "\ndf: %qd: %s\n",
			    off, strerror(nr > 0 ? EIO : errno));
		return (0);
	}
	return (1);
}

void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: %s [-hiklnP] [-t type] [[file | file_system] ...]\n",
	    __progname);
	exit(1);
}
