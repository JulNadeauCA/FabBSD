/*	$OpenBSD: dkstats.c,v 1.33 2007/11/26 09:28:34 martynas Exp $	*/
/*	$NetBSD: dkstats.c,v 1.1 1996/05/10 23:19:27 thorpej Exp $	*/

/*
 * Copyright (c) 1996 John M. Vinopal
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project
 *      by John M. Vinopal.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/dkstat.h>
#include <sys/time.h>
#include <sys/disk.h>
#include <sys/sysctl.h>
#include <sys/tty.h>

#include <err.h>
#include <fcntl.h>
#include <kvm.h>
#include <limits.h>
#include <nlist.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "dkstats.h"

#if !defined(NOKVM)
static struct nlist namelist[] = {
#define	X_TK_NIN	0		/* sysctl */
	{ "_tk_nin" },
#define	X_TK_NOUT	1		/* sysctl */
	{ "_tk_nout" },
#define	X_CP_TIME	2		/* sysctl */
	{ "_cp_time" },
#define	X_HZ		3		/* sysctl */
	{ "_hz" },
#define	X_STATHZ	4		/* sysctl */
	{ "_stathz" },
#define X_DISK_COUNT	5		/* sysctl */
	{ "_disk_count" },
#define X_DISKLIST	6		/* sysctl */
	{ "_disklist" },
	{ NULL },
};
#define	KVM_ERROR(_string) {						\
	warnx("%s", (_string));						\
	errx(1, "%s", kvm_geterr(kd));					\
}

/*
 * Dereference the namelist pointer `v' and fill in the local copy
 * 'p' which is of size 's'.
 */
#define deref_nl(v, p, s) deref_kptr((void *)namelist[(v)].n_value, (p), (s));
static void deref_kptr(void *, void *, size_t);
#endif /* !defined(NOKVM) */

/* Structures to hold the statistics. */
struct _disk	cur, last;

/* Kernel pointers: nlistf and memf defined in calling program. */
#if !defined(NOKVM)
extern kvm_t	*kd;
#endif
extern char	*nlistf;
extern char	*memf;

#if !defined(NOKVM)
/* Pointer to list of disks. */
static struct disk	*dk_drivehead = NULL;
#endif

/* Backward compatibility references. */
int		dk_ndrive = 0;
int		*dk_select;
char		**dr_name;

/* Missing from <sys/time.h> */
#define timerset(tvp, uvp) \
	((uvp)->tv_sec = (tvp)->tv_sec);		\
	((uvp)->tv_usec = (tvp)->tv_usec)

#define SWAP(fld)	tmp = cur.fld;				\
			cur.fld -= last.fld;			\
			last.fld = tmp

/*
 * Take the delta between the present values and the last recorded
 * values, storing the present values in the 'last' structure, and
 * the delta values in the 'cur' structure.
 */
void
dkswap(void)
{
	u_int64_t tmp;
	int	i;

	for (i = 0; i < cur.dk_ndrive; i++) {
		struct timeval	tmp_timer;

		if (!cur.dk_select[i])
			continue;

		/* Delta Values. */
		SWAP(dk_rxfer[i]);
		SWAP(dk_wxfer[i]);
		SWAP(dk_seek[i]);
		SWAP(dk_rbytes[i]);
		SWAP(dk_wbytes[i]);

		/* Delta Time. */
		timerclear(&tmp_timer);
		timerset(&(cur.dk_time[i]), &tmp_timer);
		timersub(&tmp_timer, &(last.dk_time[i]), &(cur.dk_time[i]));
		timerclear(&(last.dk_time[i]));
		timerset(&tmp_timer, &(last.dk_time[i]));
	}
	for (i = 0; i < CPUSTATES; i++) {
		long ltmp;

		ltmp = cur.cp_time[i];
		cur.cp_time[i] -= last.cp_time[i];
		last.cp_time[i] = ltmp;
	}
	SWAP(tk_nin);
	SWAP(tk_nout);

#undef SWAP
}

/*
 * Read the disk statistics for each disk in the disk list.
 * Also collect statistics for tty i/o and cpu ticks.
 */
void
dkreadstats(void)
{
#if !defined(NOKVM)
	struct disk	cur_disk, *p;
#endif
	int		i, j, mib[3];
	size_t		size;
	char		*disknames, *name, *bufpp, **dk_name;
	struct diskstats *q;

	last.dk_ndrive = cur.dk_ndrive;

	if (nlistf == NULL && memf == NULL) {
		/* Get the number of attached drives. */
		mib[0] = CTL_HW;
		mib[1] = HW_DISKCOUNT;
		size = sizeof(dk_ndrive);
		if (sysctl(mib, 2, &dk_ndrive, &size, NULL, 0) < 0 ) {
			warn("could not read hw.diskcount");
			dk_ndrive = 0;
		}

		if (cur.dk_ndrive != dk_ndrive) {
			/* Re-read the disk names. */
			dk_name = calloc((size_t)dk_ndrive, sizeof(char *));
			if (dk_name == NULL)
				err(1, NULL);
			mib[0] = CTL_HW;
			mib[1] = HW_DISKNAMES;
			size = 0;
			if (sysctl(mib, 2, NULL, &size, NULL, 0) < 0)
				err(1, "can't get hw.disknames");
			disknames = malloc(size);
			if (disknames == NULL)
				err(1, NULL);
			if (sysctl(mib, 2, disknames, &size, NULL, 0) < 0)
				err(1, "can't get hw.disknames");
			bufpp = disknames;
			for (i = 0; i < dk_ndrive && (name = strsep(&bufpp, ",")) != NULL; i++)
				dk_name[i] = name;
			disknames = cur.dk_name[0];	/* To free old names. */

			if (dk_ndrive < cur.dk_ndrive) {
				for (i = 0, j = 0; i < dk_ndrive; i++, j++) {
					while (j < cur.dk_ndrive &&
					    strcmp(cur.dk_name[j], dk_name[i]))
						j++;
					if (i == j) continue;

					if (j >= cur.dk_ndrive) {
						cur.dk_select[i] = 1;
						last.dk_rxfer[i] = 0;
						last.dk_wxfer[i] = 0;
						last.dk_seek[i] = 0;
						last.dk_rbytes[i] = 0;
						last.dk_wbytes[i] = 0;
						bzero(&last.dk_time[i],
						    sizeof(struct timeval));
						continue;
					}

					cur.dk_select[i] = cur.dk_select[j];
					last.dk_rxfer[i] = last.dk_rxfer[j];
					last.dk_wxfer[i] = last.dk_wxfer[j];
					last.dk_seek[i] = last.dk_seek[j];
					last.dk_rbytes[i] = last.dk_rbytes[j];
					last.dk_wbytes[i] = last.dk_wbytes[j];
					last.dk_time[i] = last.dk_time[j];
				}

				cur.dk_select = realloc(cur.dk_select,
				    dk_ndrive * sizeof(*cur.dk_select));
				cur.dk_rxfer = realloc(cur.dk_rxfer,
				    dk_ndrive * sizeof(*cur.dk_rxfer));
				cur.dk_wxfer = realloc(cur.dk_wxfer,
				    dk_ndrive * sizeof(*cur.dk_wxfer));
				cur.dk_seek = realloc(cur.dk_seek,
				    dk_ndrive * sizeof(*cur.dk_seek));
				cur.dk_rbytes = realloc(cur.dk_rbytes,
				    dk_ndrive * sizeof(*cur.dk_rbytes));
				cur.dk_wbytes = realloc(cur.dk_wbytes,
				    dk_ndrive * sizeof(*cur.dk_wbytes));
				cur.dk_time = realloc(cur.dk_time,
				    dk_ndrive * sizeof(*cur.dk_time));
				last.dk_rxfer = realloc(last.dk_rxfer,
				    dk_ndrive * sizeof(*last.dk_rxfer));
				last.dk_wxfer = realloc(last.dk_wxfer,
				    dk_ndrive * sizeof(*last.dk_wxfer));
				last.dk_seek = realloc(last.dk_seek,
				    dk_ndrive * sizeof(*last.dk_seek));
				last.dk_rbytes = realloc(last.dk_rbytes,
				    dk_ndrive * sizeof(*last.dk_rbytes));
				last.dk_wbytes = realloc(last.dk_wbytes,
				    dk_ndrive * sizeof(*last.dk_wbytes));
				last.dk_time = realloc(last.dk_time,
				    dk_ndrive * sizeof(*last.dk_time));

				if (!cur.dk_select || !cur.dk_rxfer ||
				    !cur.dk_wxfer || !cur.dk_seek ||
				    !cur.dk_rbytes || !cur.dk_wbytes ||
				    !cur.dk_time || !last.dk_rxfer ||
				    !last.dk_wxfer || !last.dk_seek ||
				    !last.dk_rbytes || !last.dk_wbytes ||
				    !last.dk_time)
					errx(1, "Memory allocation failure.");
			} else {
				cur.dk_select = realloc(cur.dk_select,
				    dk_ndrive * sizeof(*cur.dk_select));
				cur.dk_rxfer = realloc(cur.dk_rxfer,
				    dk_ndrive * sizeof(*cur.dk_rxfer));
				cur.dk_wxfer = realloc(cur.dk_wxfer,
				    dk_ndrive * sizeof(*cur.dk_wxfer));
				cur.dk_seek = realloc(cur.dk_seek,
				    dk_ndrive * sizeof(*cur.dk_seek));
				cur.dk_rbytes = realloc(cur.dk_rbytes,
				    dk_ndrive * sizeof(*cur.dk_rbytes));
				cur.dk_wbytes = realloc(cur.dk_wbytes,
				    dk_ndrive * sizeof(*cur.dk_wbytes));
				cur.dk_time = realloc(cur.dk_time,
				    dk_ndrive * sizeof(*cur.dk_time));
				last.dk_rxfer = realloc(last.dk_rxfer,
				    dk_ndrive * sizeof(*last.dk_rxfer));
				last.dk_wxfer = realloc(last.dk_wxfer,
				    dk_ndrive * sizeof(*last.dk_wxfer));
				last.dk_seek = realloc(last.dk_seek,
				    dk_ndrive * sizeof(*last.dk_seek));
				last.dk_rbytes = realloc(last.dk_rbytes,
				    dk_ndrive * sizeof(*last.dk_rbytes));
				last.dk_wbytes = realloc(last.dk_wbytes,
				    dk_ndrive * sizeof(*last.dk_wbytes));
				last.dk_time = realloc(last.dk_time,
				    dk_ndrive * sizeof(*last.dk_time));

				if (!cur.dk_select || !cur.dk_rxfer ||
				    !cur.dk_wxfer || !cur.dk_seek ||
				    !cur.dk_rbytes || !cur.dk_wbytes ||
				    !cur.dk_time || !last.dk_rxfer ||
				    !last.dk_wxfer || !last.dk_seek ||
				    !last.dk_rbytes || !last.dk_wbytes ||
				    !last.dk_time)
					errx(1, "Memory allocation failure.");

				for (i = dk_ndrive - 1, j = cur.dk_ndrive - 1;
				     i >= 0; i--) {

					if (j < 0 ||
					    strcmp(cur.dk_name[j], dk_name[i]))
					{
						cur.dk_select[i] = 1;
						last.dk_rxfer[i] = 0;
						last.dk_wxfer[i] = 0;
						last.dk_seek[i] = 0;
						last.dk_rbytes[i] = 0;
						last.dk_wbytes[i] = 0;
						bzero(&last.dk_time[i],
						    sizeof(struct timeval));
						continue;
					}

					if (i > j) {
						cur.dk_select[i] =
						    cur.dk_select[j];
						last.dk_rxfer[i] =
						    last.dk_rxfer[j];
						last.dk_wxfer[i] =
						    last.dk_wxfer[j];
						last.dk_seek[i] =
						    last.dk_seek[j];
						last.dk_rbytes[i] =
						    last.dk_rbytes[j];
						last.dk_wbytes[i] =
						    last.dk_wbytes[j];
						last.dk_time[i] =
						    last.dk_time[j];
					}
					j--;
				}
			}

			cur.dk_ndrive = dk_ndrive;
			free(disknames);
			cur.dk_name = dk_name;
			dr_name = cur.dk_name;
			dk_select = cur.dk_select;
		}

		size = cur.dk_ndrive * sizeof(struct diskstats);
		mib[0] = CTL_HW;
		mib[1] = HW_DISKSTATS;
		q = malloc(size);
		if (q == NULL)
			err(1, NULL);
		if (sysctl(mib, 2, q, &size, NULL, 0) < 0) {
#ifdef	DEBUG
			warn("could not read hw.diskstats");
#endif	/* DEBUG */
			bzero(q, cur.dk_ndrive * sizeof(struct diskstats));
		}

		for (i = 0; i < cur.dk_ndrive; i++)	{
			cur.dk_rxfer[i] = q[i].ds_rxfer;
			cur.dk_wxfer[i] = q[i].ds_wxfer;
			cur.dk_seek[i] = q[i].ds_seek;
			cur.dk_rbytes[i] = q[i].ds_rbytes;
			cur.dk_wbytes[i] = q[i].ds_wbytes;
			timerset(&(q[i].ds_time), &(cur.dk_time[i]));
		}
		free(q);

		size = sizeof(cur.cp_time);
		mib[0] = CTL_KERN;
		mib[1] = KERN_CPTIME;
		if (sysctl(mib, 2, cur.cp_time, &size, NULL, 0) < 0) {
			warn("could not read kern.cp_time");
			bzero(cur.cp_time, sizeof(cur.cp_time));
		}
		size = sizeof(cur.tk_nin);
		mib[0] = CTL_KERN;
		mib[1] = KERN_TTY;
		mib[2] = KERN_TTY_TKNIN;
		if (sysctl(mib, 3, &cur.tk_nin, &size, NULL, 0) < 0) {
			warn("could not read kern.tty.tk_nin");
			cur.tk_nin = 0;
		}
		size = sizeof(cur.tk_nin);
		mib[0] = CTL_KERN;
		mib[1] = KERN_TTY;
		mib[2] = KERN_TTY_TKNOUT;
		if (sysctl(mib, 3, &cur.tk_nout, &size, NULL, 0) < 0) {
			warn("could not read kern.tty.tk_nout");
			cur.tk_nout = 0;
		}
	} else {
#if !defined(NOKVM)
		p = dk_drivehead;

		for (i = 0; i < cur.dk_ndrive; i++) {
			deref_kptr(p, &cur_disk, sizeof(cur_disk));
			cur.dk_rxfer[i] = cur_disk.dk_rxfer;
			cur.dk_wxfer[i] = cur_disk.dk_wxfer;
			cur.dk_seek[i] = cur_disk.dk_seek;
			cur.dk_rbytes[i] = cur_disk.dk_rbytes;
			cur.dk_wbytes[i] = cur_disk.dk_wbytes;
			timerset(&(cur_disk.dk_time), &(cur.dk_time[i]));
			p = TAILQ_NEXT(&cur_disk, dk_link);
		}
		deref_nl(X_CP_TIME, cur.cp_time, sizeof(cur.cp_time));
		deref_nl(X_TK_NIN, &cur.tk_nin, sizeof(cur.tk_nin));
		deref_nl(X_TK_NOUT, &cur.tk_nout, sizeof(cur.tk_nout));
#endif /* !defined(NOKVM) */
	}
}

/*
 * Perform all of the initialization and memory allocation needed to
 * track disk statistics.
 */
int
dkinit(int sel)
{
#if !defined(NOKVM)
	struct disklist_head disk_head;
	struct disk	cur_disk, *p;
        char		errbuf[_POSIX2_LINE_MAX];
#endif
	static int	once = 0;
	extern int	hz;
	int		i, mib[2];
	size_t		size;
	struct clockinfo clkinfo;
	char		*disknames, *name, *bufpp;
	gid_t		gid;

	if (once)
		return(1);

	gid = getgid();
	if (nlistf != NULL || memf != NULL) {
#if !defined(NOKVM)
		if (memf != NULL)
			if (setresgid(gid, gid, gid) == -1)
				err(1, "setresgid");

		/* Open the kernel. */
		if (kd == NULL &&
		    (kd = kvm_openfiles(nlistf, memf, NULL, O_RDONLY,
		    errbuf)) == NULL)
			errx(1, "kvm_openfiles: %s", errbuf);

		if (memf == NULL)
			if (setresgid(gid, gid, gid) == -1)
				err(1, "setresgid");

		/* Obtain the namelist symbols from the kernel. */
		if (kvm_nlist(kd, namelist))
			KVM_ERROR("kvm_nlist failed to read symbols.");

		/* Get the number of attached drives. */
		deref_nl(X_DISK_COUNT, &cur.dk_ndrive, sizeof(cur.dk_ndrive));

		if (cur.dk_ndrive < 0)
			errx(1, "invalid _disk_count %d.", cur.dk_ndrive);

		/* Get a pointer to the first disk. */
		deref_nl(X_DISKLIST, &disk_head, sizeof(disk_head));
		dk_drivehead = TAILQ_FIRST(&disk_head);

		/* Get ticks per second. */
		deref_nl(X_STATHZ, &hz, sizeof(hz));
		if (!hz)
		  deref_nl(X_HZ, &hz, sizeof(hz));
#endif /* !defined(NOKVM) */
	} else {
		/* Get the number of attached drives. */
		mib[0] = CTL_HW;
		mib[1] = HW_DISKCOUNT;
		size = sizeof(cur.dk_ndrive);
		if (sysctl(mib, 2, &cur.dk_ndrive, &size, NULL, 0) < 0 ) {
			warn("could not read hw.diskcount");
			cur.dk_ndrive = 0;
		}

		/* Get ticks per second. */
		mib[0] = CTL_KERN;
		mib[1] = KERN_CLOCKRATE;
		size = sizeof(clkinfo);
		if (sysctl(mib, 2, &clkinfo, &size, NULL, 0) < 0) {
			warn("could not read kern.clockrate");
			hz = 0;
		} else
			hz = clkinfo.stathz;
	}

	/* allocate space for the statistics */
	cur.dk_time = calloc((size_t)cur.dk_ndrive, sizeof(struct timeval));
	cur.dk_rxfer = calloc((size_t)cur.dk_ndrive, sizeof(u_int64_t));
	cur.dk_wxfer = calloc((size_t)cur.dk_ndrive, sizeof(u_int64_t));
	cur.dk_seek = calloc((size_t)cur.dk_ndrive, sizeof(u_int64_t));
	cur.dk_rbytes = calloc((size_t)cur.dk_ndrive, sizeof(u_int64_t));
	cur.dk_wbytes = calloc((size_t)cur.dk_ndrive, sizeof(u_int64_t));
	cur.dk_select = calloc((size_t)cur.dk_ndrive, sizeof(int));
	cur.dk_name = calloc((size_t)cur.dk_ndrive, sizeof(char *));
	last.dk_time = calloc((size_t)cur.dk_ndrive, sizeof(struct timeval));
	last.dk_rxfer = calloc((size_t)cur.dk_ndrive, sizeof(u_int64_t));
	last.dk_wxfer = calloc((size_t)cur.dk_ndrive, sizeof(u_int64_t));
	last.dk_seek = calloc((size_t)cur.dk_ndrive, sizeof(u_int64_t));
	last.dk_rbytes = calloc((size_t)cur.dk_ndrive, sizeof(u_int64_t));
	last.dk_wbytes = calloc((size_t)cur.dk_ndrive, sizeof(u_int64_t));

	if (!cur.dk_time || !cur.dk_rxfer || !cur.dk_wxfer || !cur.dk_seek ||
	    !cur.dk_rbytes || !cur.dk_wbytes || !cur.dk_select ||
	    !cur.dk_name || !last.dk_time || !last.dk_rxfer ||
	    !last.dk_wxfer || !last.dk_seek || !last.dk_rbytes ||
	    !last.dk_wbytes)
		errx(1, "Memory allocation failure.");

	/* Set up the compatibility interfaces. */
	dk_ndrive = cur.dk_ndrive;
	dk_select = cur.dk_select;
	dr_name = cur.dk_name;

	/* Read the disk names and set initial selection. */
	if (nlistf == NULL && memf == NULL) {
		mib[0] = CTL_HW;
		mib[1] = HW_DISKNAMES;
		size = 0;
		if (sysctl(mib, 2, NULL, &size, NULL, 0) < 0)
			err(1, "can't get hw.disknames");
		disknames = malloc(size);
		if (disknames == NULL)
			err(1, NULL);
		if (sysctl(mib, 2, disknames, &size, NULL, 0) < 0)
			err(1, "can't get hw.disknames");
		bufpp = disknames;
		for (i = 0; i < dk_ndrive && (name = strsep(&bufpp, ",")) != NULL; i++) {
			cur.dk_name[i] = name;
			cur.dk_select[i] = sel;
		}
	} else {
#if !defined(NOKVM)
		p = dk_drivehead;
		for (i = 0; i < cur.dk_ndrive; i++) {
			char	buf[10];

			deref_kptr(p, &cur_disk, sizeof(cur_disk));
			deref_kptr(cur_disk.dk_name, buf, sizeof(buf));
			cur.dk_name[i] = strdup(buf);
			if (!cur.dk_name[i])
				errx(1, "Memory allocation failure.");
			cur.dk_select[i] = sel;

			p = TAILQ_NEXT(&cur_disk, dk_link);
		}
#endif /* !defined(NOKVM) */
	}

	/* Never do this initialization again. */
	once = 1;
	return(1);
}

#if !defined(NOKVM)
/*
 * Dereference the kernel pointer `kptr' and fill in the local copy
 * pointed to by `ptr'.  The storage space must be pre-allocated,
 * and the size of the copy passed in `len'.
 */
static void
deref_kptr(void *kptr, void *ptr, size_t len)
{
	char buf[128];

	if (kvm_read(kd, (u_long)kptr, ptr, len) != len) {
		bzero(buf, sizeof(buf));
		snprintf(buf, (sizeof(buf) - 1),
		     "can't dereference kptr 0x%lx", (u_long)kptr);
		KVM_ERROR(buf);
	}
}
#endif /* !defined(NOKVM) */
