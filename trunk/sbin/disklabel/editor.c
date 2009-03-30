/*	$OpenBSD: editor.c,v 1.168 2008/06/25 18:31:07 otto Exp $	*/

/*
 * Copyright (c) 1997-2000 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef lint
static char rcsid[] = "$OpenBSD: editor.c,v 1.168 2008/06/25 18:31:07 otto Exp $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#define	DKTYPENAMES
#include <sys/disklabel.h>

#include <ufs/ffs/fs.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <string.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "extern.h"
#include "pathnames.h"

/* flags for getuint() */
#define	DO_CONVERSIONS	0x00000001
#define	DO_ROUNDING	0x00000002

#ifndef NUMBOOT
#define NUMBOOT 0
#endif

/* structure to describe a portion of a disk */
struct diskchunk {
	u_int64_t start;
	u_int64_t stop;
};

/* used when sorting mountpoints in mpsave() */
struct mountinfo {
	char *mountpoint;
	int partno;
};

void	edit_parms(struct disklabel *);
void	editor_add(struct disklabel *, char **, char *);
void	editor_change(struct disklabel *, char *);
u_int64_t editor_countfree(struct disklabel *);
void	editor_delete(struct disklabel *, char **, char *);
void	editor_help(char *);
void	editor_modify(struct disklabel *, char **, char *);
void	editor_name(struct disklabel *, char **, char *);
char	*getstring(char *, char *, char *);
u_int64_t getuint(struct disklabel *, char *, char *, u_int64_t, u_int64_t, u_int64_t, int);
int	has_overlap(struct disklabel *);
int	partition_cmp(const void *, const void *);
struct partition **sort_partitions(struct disklabel *);
void	getdisktype(struct disklabel *, char *, char *);
void	find_bounds(struct disklabel *);
void	set_bounds(struct disklabel *);
struct diskchunk *free_chunks(struct disklabel *);
char **	mpcopy(char **, char **);
int	micmp(const void *, const void *);
int	mpequal(char **, char **);
int	mpsave(struct disklabel *, char **, char *, char *);
int	get_bsize(struct disklabel *, int);
int	get_fsize(struct disklabel *, int);
int	get_fstype(struct disklabel *, int);
int	get_mp(struct disklabel *, char **, int);
int	get_offset(struct disklabel *, int);
int	get_size(struct disklabel *, int);
void	get_geometry(int, struct disklabel **);
void	set_geometry(struct disklabel *, struct disklabel *, struct disklabel *,
	    char *);
void	zero_partitions(struct disklabel *);
u_int64_t max_partition_size(struct disklabel *, int);
void	display_edit(struct disklabel *, char **, char, u_int64_t);

static u_int64_t starting_sector;
static u_int64_t ending_sector;
static int expert;

/*
 * Simple partition editor.  Primarily intended for new labels.
 */
int
editor(struct disklabel *lp, int f, char *dev, char *fstabfile)
{
	struct disklabel lastlabel, tmplabel, label = *lp;
	struct disklabel *disk_geop;
	struct partition *pp;
	FILE *fp;
	char buf[BUFSIZ], *cmd, *arg;
	char **mountpoints = NULL, **omountpoints = NULL, **tmpmountpoints = NULL;

	/* Alloc and init mount point info */
	if (fstabfile) {
		if (!(mountpoints = calloc(MAXPARTITIONS, sizeof(char *))) ||
		    !(omountpoints = calloc(MAXPARTITIONS, sizeof(char *))) ||
		    !(tmpmountpoints = calloc(MAXPARTITIONS, sizeof(char *))))
			errx(4, "out of memory");
	}

	/* Don't allow disk type of "unknown" */
	getdisktype(&label, "You need to specify a type for this disk.", dev);

	/* Get the on-disk geometries if possible */
	get_geometry(f, &disk_geop);

	/* How big is the OpenBSD portion of the disk?  */
	find_bounds(&label);

	/* Make sure there is no partition overlap. */
	if (has_overlap(&label))
		errx(1, "can't run when there is partition overlap.");

	/* If we don't have a 'c' partition, create one. */
	pp = &label.d_partitions[RAW_PART];
	if (label.d_npartitions < 3 || DL_GETPSIZE(pp) == 0) {
		puts("No 'c' partition found, adding one that spans the disk.");
		if (label.d_npartitions < 3)
			label.d_npartitions = 3;
		DL_SETPOFFSET(pp, 0);
		DL_SETPSIZE(pp, DL_GETDSIZE(&label));
		pp->p_fstype = FS_UNUSED;
		pp->p_fragblock = pp->p_cpg = 0;
	}

#ifdef SUN_CYLCHECK
	if (label.d_flags & D_VENDOR) {
		puts("This platform requires that partition offsets/sizes "
		    "be on cylinder boundaries.\n"
		    "Partition offsets/sizes will be rounded to the "
		    "nearest cylinder automatically.");
	}
#endif

	/* Set d_bbsize and d_sbsize as necessary */
	if (label.d_bbsize == 0)
		label.d_bbsize = BBSIZE;
	if (label.d_sbsize == 0)
		label.d_sbsize = SBSIZE;

	/* Interleave must be >= 1 */
	if (label.d_interleave == 0)
		label.d_interleave = 1;

	puts("Initial label editor (enter '?' for help at any prompt)");
	lastlabel = label;
	for (;;) {
		fputs("> ", stdout);
		if (fgets(buf, sizeof(buf), stdin) == NULL) {
			putchar('\n');
			buf[0] = 'q';
			buf[1] = '\0';
		}
		if ((cmd = strtok(buf, " \t\r\n")) == NULL)
			continue;
		arg = strtok(NULL, " \t\r\n");

		switch (*cmd) {

		case '?':
		case 'h':
			editor_help(arg ? arg : "");
			break;

		case 'a':
			tmplabel = lastlabel;
			lastlabel = label;
			if (mountpoints != NULL) {
				mpcopy(tmpmountpoints, omountpoints);
				mpcopy(omountpoints, mountpoints);
			}
			editor_add(&label, mountpoints, arg);
			if (memcmp(&label, &lastlabel, sizeof(label)) == 0)
				lastlabel = tmplabel;
			if (mountpoints != NULL && mpequal(omountpoints, tmpmountpoints))
				mpcopy(omountpoints, tmpmountpoints);
			break;

		case 'b':
			tmplabel = lastlabel;
			lastlabel = label;
			set_bounds(&label);
			if (memcmp(&label, &lastlabel, sizeof(label)) == 0)
				lastlabel = tmplabel;
			break;

		case 'c':
			tmplabel = lastlabel;
			lastlabel = label;
			editor_change(&label, arg);
			if (memcmp(&label, &lastlabel, sizeof(label)) == 0)
				lastlabel = tmplabel;
			break;

		case 'D':
			tmplabel = lastlabel;
			lastlabel = label;
			if (ioctl(f, DIOCGPDINFO, &label) == 0) {
				dflag = 1;
			} else {
				warn("unable to get default partition table");
				lastlabel = tmplabel;
			}
			break;

		case 'd':
			tmplabel = lastlabel;
			lastlabel = label;
			if (mountpoints != NULL) {
				mpcopy(tmpmountpoints, omountpoints);
				mpcopy(omountpoints, mountpoints);
			}
			editor_delete(&label, mountpoints, arg);
			if (memcmp(&label, &lastlabel, sizeof(label)) == 0)
				lastlabel = tmplabel;
			if (mountpoints != NULL && mpequal(omountpoints, tmpmountpoints))
				mpcopy(omountpoints, tmpmountpoints);
			break;

		case 'e':
			tmplabel = lastlabel;
			lastlabel = label;
			edit_parms(&label);
			if (memcmp(&label, &lastlabel, sizeof(label)) == 0)
				lastlabel = tmplabel;
			break;

		case 'g':
			tmplabel = lastlabel;
			lastlabel = label;
			set_geometry(&label, disk_geop, lp, arg);
			if (memcmp(&label, &lastlabel, sizeof(label)) == 0)
				lastlabel = tmplabel;
			break;

		case 'm':
			tmplabel = lastlabel;
			lastlabel = label;
			if (mountpoints != NULL) {
				mpcopy(tmpmountpoints, omountpoints);
				mpcopy(omountpoints, mountpoints);
			}
			editor_modify(&label, mountpoints, arg);
			if (memcmp(&label, &lastlabel, sizeof(label)) == 0)
				lastlabel = tmplabel;
			if (mountpoints != NULL && mpequal(omountpoints, tmpmountpoints))
				mpcopy(omountpoints, tmpmountpoints);
			break;

		case 'n':
			if (mountpoints == NULL) {
				fputs("This option is not valid when run "
				    "without the -f flag.\n", stderr);
				break;
			}
			mpcopy(tmpmountpoints, omountpoints);
			mpcopy(omountpoints, mountpoints);
			editor_name(&label, mountpoints, arg);
			if (mpequal(omountpoints, tmpmountpoints))
				mpcopy(omountpoints, tmpmountpoints);
			break;

		case 'p':
			display_edit(&label, mountpoints, arg ? *arg : 0,
			    editor_countfree(&label));
			break;

		case 'l':
			display(stdout, &label, mountpoints, arg ? *arg : 0, 0);
			break;

		case 'M': {
			sig_t opipe = signal(SIGPIPE, SIG_IGN);
			char *pager, *cmd = NULL;
			extern const u_char manpage[];
			extern const int manpage_sz;

			if ((pager = getenv("PAGER")) == NULL || *pager == '\0')
				pager = _PATH_LESS;

			if (asprintf(&cmd, "gunzip -qc|%s", pager) != -1 &&
			    (fp = popen(cmd, "w")) != NULL) {
				(void) fwrite(manpage, manpage_sz, 1, fp);
				pclose(fp);
			} else
				warn("unable to execute %s", pager);

			free(cmd);
			(void)signal(SIGPIPE, opipe);
			break;
		}

		case 'q':
			if (donothing) {
				puts("In no change mode, not writing label.");
				return(1);
			}
			/* Save mountpoint info if there is any. */
			if (mountpoints != NULL)
				mpsave(&label, mountpoints, dev, fstabfile);
			/*
			 * If we didn't manufacture a new default label and
			 * didn't change the label read from disk, there is no
			 * need to do anything before exiting.
			 */
			if (!dflag && memcmp(lp, &label, sizeof(label)) == 0) {
				puts("No label changes.");
				return(1);
			}
			do {
				arg = getstring("Write new label?",
				    "Write the modified label to disk?",
				    "y");
			} while (arg && tolower(*arg) != 'y' && tolower(*arg) != 'n');
			if (arg && tolower(*arg) == 'y') {
				if (writelabel(f, bootarea, &label) == 0) {
					*lp = label;
					return(0);
				}
				warnx("unable to write label");
			}
			return(1);
			/* NOTREACHED */
			break;

		case 'r': {
			struct diskchunk *chunks;
			int i;
			/* Display free space. */
			chunks = free_chunks(&label);
			for (i = 0; chunks[i].start != 0 || chunks[i].stop != 0;
			    i++)
				fprintf(stderr, "Free sectors: %16llu - %16llu "
				    "(%16llu)\n",
			    	    chunks[i].start, chunks[i].stop - 1,
			   	    chunks[i].stop - chunks[i].start);
			fprintf(stderr, "Total free sectors: %llu.\n",
			    editor_countfree(&label));
		    	break;
		}

		case 's':
			if (arg == NULL) {
				arg = getstring("Filename",
				    "Name of the file to save label into.",
				    NULL);
				if (arg == NULL && *arg == '\0')
					break;
			}
			if ((fp = fopen(arg, "w")) == NULL) {
				warn("cannot open %s", arg);
			} else {
				display(fp, &label, NULL, 0, 1) ;
				(void)fclose(fp);
			}
			break;

		case 'u':
			if (memcmp(&label, &lastlabel, sizeof(label)) == 0 &&
			    mountpoints != NULL &&
			    mpequal(mountpoints, omountpoints)) {
				puts("Nothing to undo!");
			} else {
				tmplabel = label;
				label = lastlabel;
				lastlabel = tmplabel;
				/* Restore mountpoints */
				if (mountpoints != NULL)
					mpcopy(mountpoints, omountpoints);
				puts("Last change undone.");
			}
			break;

		case 'w':
			if (donothing)  {
				puts("In no change mode, not writing label.");
				break;
			}
			/* Save mountpoint info if there is any. */
			if (mountpoints != NULL)
				mpsave(&label, mountpoints, dev, fstabfile);
			/* Write label to disk. */
			if (writelabel(f, bootarea, &label) != 0)
				warnx("unable to write label");
			else {
				dflag = 0;
				*lp = label;
			}
			break;

		case 'X':
			expert = !expert;
			printf("%s expert mode\n", expert ? "Entering" :
			    "Exiting");
			break;

		case 'x':
			return(1);
			break;

		case 'z':
			tmplabel = lastlabel;
			lastlabel = label;
			zero_partitions(&label);
			break;

		case '\n':
			break;

		default:
			printf("Unknown option: %c ('?' for help)\n", *cmd);
			break;
		}
	}
}

/*
 * Add a new partition.
 */
void
editor_add(struct disklabel *lp, char **mp, char *p)
{
	struct partition *pp;
	struct diskchunk *chunks;
	char buf[2];
	int i, partno;
	u_int64_t freesectors, new_offset, new_size;

	freesectors = editor_countfree(lp);

	/* XXX - prompt user to steal space from another partition instead */
#ifdef SUN_CYLCHECK
	if ((lp->d_flags & D_VENDOR) && freesectors < lp->d_secpercyl) {
		fputs("No space left, you need to shrink a partition "
		    "(need at least one full cylinder)\n",
		    stderr);
		return;
	}
#endif
	if (freesectors == 0) {
		fputs("No space left, you need to shrink a partition\n",
		    stderr);
		return;
	}

	if (p == NULL) {
		/*
		 * Use the first unused partition that is not 'c' as the
		 * default partition in the prompt string.
		 */
		pp = &lp->d_partitions[0];
		buf[0] = buf[1] = '\0';
		for (partno = 0; partno < MAXPARTITIONS; partno++, pp++) {
			if (DL_GETPSIZE(pp) == 0 && partno != RAW_PART) {
				buf[0] = partno + 'a';
				p = &buf[0];
				break;
			}
		}
		p = getstring("partition",
		    "The letter of the new partition, a - p.", p);
	}
	if (p == NULL) {
		fputs("Command aborted\n", stderr);
		return;
	}
	partno = p[0] - 'a';
	if (partno < 0 || partno == RAW_PART || partno >= MAXPARTITIONS) {
		fprintf(stderr, "Partition must be between 'a' and '%c' "
		    "(excluding 'c').\n", 'a' + MAXPARTITIONS - 1);
		return;
	}	
	pp = &lp->d_partitions[partno];

	if (pp->p_fstype != FS_UNUSED && DL_GETPSIZE(pp) != 0) {
		fprintf(stderr, "Partition '%c' exists.  Delete it first.\n",
		    p[0]);
		return;
	}

	/*
	 * Increase d_npartitions if necessary. Ensure all new partitions are
	 * zero'ed to avoid inadvertant overlaps.
	 */
	for(; lp->d_npartitions <= partno; lp->d_npartitions++)
		memset(&lp->d_partitions[lp->d_npartitions], 0, sizeof(*pp));

	/* Make sure selected partition is zero'd too. */
	memset(pp, 0, sizeof(*pp)); 
	chunks = free_chunks(lp);

	/*
	 * Since we know there's free space, there must be at least one
	 * chunk. So find the largest chunk and assume we want to add the
	 * partition in that free space.
	 */
	new_size = new_offset = 0;
	for (i = 0; chunks[i].start != 0 || chunks[i].stop != 0; i++) {
		if (chunks[i].stop - chunks[i].start > new_size) {
		    new_size = chunks[i].stop - chunks[i].start;
		    new_offset = chunks[i].start;
		}
	}
	DL_SETPSIZE(pp, new_size);
	DL_SETPOFFSET(pp, new_offset);
	pp->p_fstype = partno == 1 ? FS_SWAP : FS_BSDFFS;
#if defined (__sparc__) && !defined(__sparc64__)
	/* can't boot from > 8k boot blocks */
	pp->p_fragblock =
	    DISKLABELV1_FFS_FRAGBLOCK(partno == 0 ? 1024 : 2048, 8);
#else
	pp->p_fragblock = DISKLABELV1_FFS_FRAGBLOCK(2048, 8);
#endif
	pp->p_cpg = 1;

	if (get_offset(lp, partno) == 0 &&
	    get_size(lp, partno) == 0   &&
	    get_fstype(lp, partno) == 0 &&
	    get_mp(lp, mp, partno) == 0 &&
	    get_fsize(lp, partno) == 0  &&
	    get_bsize(lp, partno) == 0)
		return;

	/* Bailed out at some point, so effectively delete the partition. */
	DL_SETPSIZE(pp, 0);
}

/*
 * Set the mountpoint of an existing partition ('name').
 */
void
editor_name(struct disklabel *lp, char **mp, char *p)
{
	struct partition *pp;
	int partno;

	/* Change which partition? */
	if (p == NULL) {
		p = getstring("partition to name",
		    "The letter of the partition to name, a - p.", NULL);
	}
	if (p == NULL) {
		fputs("Command aborted\n", stderr);
		return;
	}
	partno = p[0] - 'a';
	if (partno < 0 || partno == RAW_PART || partno >= lp->d_npartitions) {
		fprintf(stderr, "Partition must be between 'a' and '%c' "
		    "(excluding 'c').\n", 'a' + lp->d_npartitions - 1);
		return;
	}
	pp = &lp->d_partitions[partno];

	if (pp->p_fstype == FS_UNUSED && DL_GETPSIZE(pp) == 0) {
		fprintf(stderr, "Partition '%c' is not in use.\n", p[0]);
		return;
	}

	/* Not all fstypes can be named */
	if (pp->p_fstype == FS_UNUSED || pp->p_fstype == FS_SWAP ||
	    pp->p_fstype == FS_BOOT || pp->p_fstype == FS_OTHER ||
	    pp->p_fstype == FS_RAID) {
		fprintf(stderr, "You cannot name a filesystem of type %s.\n",
		    fstypenames[lp->d_partitions[partno].p_fstype]);
		return;
	}

	get_mp(lp, mp, partno);
}

/*
 * Change an existing partition.
 */
void
editor_modify(struct disklabel *lp, char **mp, char *p)
{
	struct partition origpart, *pp;
	int partno;

	/* Change which partition? */
	if (p == NULL) {
		p = getstring("partition to modify",
		    "The letter of the partition to modify, a - p.", NULL);
	}
	if (p == NULL) {
		fputs("Command aborted\n", stderr);
		return;
	}
	partno = p[0] - 'a';
	if (partno < 0 || partno == RAW_PART || partno >= lp->d_npartitions) {
		fprintf(stderr, "Partition must be between 'a' and '%c' "
		    "(excluding 'c').\n", 'a' + lp->d_npartitions - 1);
		return;
	}
	pp = &lp->d_partitions[partno];

	if (pp->p_fstype == FS_UNUSED && DL_GETPSIZE(pp) == 0) {
		fprintf(stderr, "Partition '%c' is not in use.\n", p[0]);
		return;
	}

	origpart = *pp;

	if (get_offset(lp, partno) == 0 &&
	    get_size(lp, partno) == 0   &&
	    get_fstype(lp, partno) == 0 &&
	    get_mp(lp, mp, partno) == 0 &&
	    get_fsize(lp, partno) == 0  &&
	    get_bsize(lp, partno) == 0)
		return;

	/* Bailed out at some point, so undo any changes. */
	*pp = origpart;
}

/*
 * Delete an existing partition.
 */
void
editor_delete(struct disklabel *lp, char **mp, char *p)
{
	struct partition *pp;
	int partno;

	if (p == NULL) {
		p = getstring("partition to delete",
		    "The letter of the partition to delete, a - p, or '*'.",
		    NULL);
	}
	if (p == NULL) {
		fputs("Command aborted\n", stderr);
		return;
	}
	if (p[0] == '*') {
		zero_partitions(lp);
		return;
	}
	partno = p[0] - 'a';
	if (partno < 0 || partno == RAW_PART || partno >= lp->d_npartitions) {
		fprintf(stderr, "Partition must be between 'a' and '%c' "
		    "(excluding 'c').\n", 'a' + lp->d_npartitions - 1);
		return;
	}
	pp = &lp->d_partitions[partno];

	if (pp->p_fstype == FS_UNUSED && DL_GETPSIZE(pp) == 0) {
		fprintf(stderr, "Partition '%c' is not in use.\n", p[0]);
		return;
	}

	/* Really delete it (as opposed to just setting to "unused") */
	memset(pp, 0, sizeof(*pp));

	if (mp != NULL && mp[partno] != NULL) {
		free(mp[partno]);
		mp[partno] = NULL;
	}
}

/*
 * Change the size of an existing partition.
 */
void
editor_change(struct disklabel *lp, char *p)
{
	struct partition *pp;
	int partno;

	if (p == NULL) {
		p = getstring("partition to change size",
		    "The letter of the partition to change size, a - p.", NULL);
	}
	if (p == NULL) {
		fputs("Command aborted\n", stderr);
		return;
	}
	partno = p[0] - 'a';
	if (partno < 0 || partno == RAW_PART || partno >= lp->d_npartitions) {
		fprintf(stderr, "Partition must be between 'a' and '%c' "
		    "(excluding 'c').\n", 'a' + lp->d_npartitions - 1);
		return;
	}
	pp = &lp->d_partitions[partno];

	if (DL_GETPSIZE(pp) == 0) {
		fprintf(stderr, "Partition '%c' is not in use.\n", p[0]);
		return;
	}

	printf("Partition %c is currently %llu sectors in size, and can have "
	    "a maximum\nsize of %llu sectors.\n",
	    p[0], DL_GETPSIZE(pp), max_partition_size(lp, partno));

	/* Get new size */
	get_size(lp, partno);
}

/*
 * Sort the partitions based on starting offset.
 * This assumes there can be no overlap.
 */
int
partition_cmp(const void *e1, const void *e2)
{
	struct partition *p1 = *(struct partition **)e1;
	struct partition *p2 = *(struct partition **)e2;
	u_int64_t o1 = DL_GETPOFFSET(p1);
	u_int64_t o2 = DL_GETPOFFSET(p2);

	if (o1 < o2)
		return -1;
	else if (o1 > o2)
		return 1;
	else
		return 0;
}

char *
getstring(char *prompt, char *helpstring, char *oval)
{
	static char buf[BUFSIZ];
	int n;

	buf[0] = '\0';
	do {
		printf("%s: [%s] ", prompt, oval ? oval : "");
		if (fgets(buf, sizeof(buf), stdin) == NULL) {
			buf[0] = '\0';
			if (feof(stdin)) {
				clearerr(stdin);
				putchar('\n');
				return(NULL);
			}
		}
		n = strlen(buf);
		if (n > 0 && buf[n-1] == '\n')
			buf[--n] = '\0';
		if (buf[0] == '?')
			puts(helpstring);
		else if (oval != NULL && buf[0] == '\0')
			strlcpy(buf, oval, sizeof(buf));
	} while (buf[0] == '?');

	return(&buf[0]);
}

/*
 * Returns ULLONG_MAX on error
 * Usually only called by helper functions.
 */
u_int64_t
getuint(struct disklabel *lp, char *prompt, char *helpstring,
    u_int64_t oval, u_int64_t maxval, u_int64_t offset, int flags)
{
	char buf[BUFSIZ], *endptr, *p, operator = '\0';
	u_int64_t rval = oval;
	size_t n;
	int mult = 1;
	double d, percent = 1.0;

	/* We only care about the remainder */
	offset = offset % lp->d_secpercyl;

	buf[0] = '\0';
	do {
		printf("%s: [%llu] ", prompt, oval);
		if (fgets(buf, sizeof(buf), stdin) == NULL) {
			buf[0] = '\0';
			if (feof(stdin)) {
				clearerr(stdin);
				putchar('\n');
				return(ULLONG_MAX - 1);
			}
		}
		n = strlen(buf);
		if (n > 0 && buf[n-1] == '\n')
			buf[--n] = '\0';
		if (buf[0] == '?')
			puts(helpstring);
	} while (buf[0] == '?');

	if (buf[0] == '*' && buf[1] == '\0') {
		rval = maxval;
	} else {
		/* deal with units */
		if (buf[0] != '\0' && n > 0) {
			if ((flags & DO_CONVERSIONS)) {
				switch (tolower(buf[n-1])) {

				case 'c':
					mult = lp->d_secpercyl;
					buf[--n] = '\0';
					break;
				case 'b':
					mult = -lp->d_secsize;
					buf[--n] = '\0';
					break;
				case 'k':
					if (lp->d_secsize > 1024)
						mult = -lp->d_secsize / 1024;
					else
						mult = 1024 / lp->d_secsize;
					buf[--n] = '\0';
					break;
				case 'm':
					mult = 1048576 / lp->d_secsize;
					buf[--n] = '\0';
					break;
				case 'g':
					mult = 1073741824 / lp->d_secsize;
					buf[--n] = '\0';
					break;
				case '%':
					buf[--n] = '\0';
					percent = strtod(buf, NULL) / 100.0;
					snprintf(buf, sizeof(buf), "%lld",
					    DL_GETDSIZE(lp));
					break;
				case '&':
					buf[--n] = '\0';
					percent = strtod(buf, NULL) / 100.0;
					snprintf(buf, sizeof(buf), "%lld",
					    maxval);
					break;
				}
			}

			/* Did they give us an operator? */
			p = &buf[0];
			if (*p == '+' || *p == '-')
				operator = *p++;

			endptr = p;
			errno = 0;
			d = strtod(p, &endptr);
			if (errno == ERANGE)
				rval = ULLONG_MAX;	/* too big/small */
			else if (*endptr != '\0') {
				errno = EINVAL;		/* non-numbers in str */
				rval = ULLONG_MAX;
			} else {
				/* XXX - should check for overflow */
				if (mult > 0)
					rval = d * mult * percent;
				else
					/* Negative mult means divide (fancy) */
					rval = d / (-mult) * percent;

				/* Apply the operator */
				if (operator == '+')
					rval += oval;
				else if (operator == '-')
					rval = oval - rval;
			}
		}
	}
	if ((flags & DO_ROUNDING) && rval != ULLONG_MAX) {
		/* Round to nearest cylinder unless given in sectors */
		if (
#ifdef SUN_CYLCHECK
		    ((lp->d_flags & D_VENDOR) || mult != 1) &&
#else
		    mult != 1 &&
#endif
		    (rval + offset) % lp->d_secpercyl != 0) {
			u_int64_t cyls;

			/* Round to higher cylinder but no more than maxval */
			cyls = (rval / lp->d_secpercyl) + 1;
			if ((cyls * lp->d_secpercyl) - offset > maxval)
				cyls--;
			rval = (cyls * lp->d_secpercyl) - offset;
			printf("Rounding to cylinder: %llu\n", rval);
		}
	}

	return(rval);
}

/*
 * Check for partition overlap in lp and prompt the user to resolve the overlap
 * if any is found.  Returns 1 if unable to resolve, else 0.
 */
int
has_overlap(struct disklabel *lp)
{
	struct partition **spp;
	int c, i, j;
	char buf[BUFSIZ];

	/* Get a sorted list of the in-use partitions. */
	spp = sort_partitions(lp);

	/* If there are less than two partitions in use, there is no overlap. */
	if (spp[1] == NULL)
		return(0);

	/* Now that we have things sorted by starting sector check overlap */
	for (i = 0; spp[i] != NULL; i++) {
		for (j = i + 1; spp[j] != NULL; j++) {
			/* `if last_sec_in_part + 1 > first_sec_in_next_part' */
			if (DL_GETPOFFSET(spp[i]) + DL_GETPSIZE(spp[i]) > DL_GETPOFFSET(spp[j])) {
				/* Overlap!  Convert to real part numbers. */
				i = ((char *)spp[i] - (char *)lp->d_partitions)
				    / sizeof(**spp);
				j = ((char *)spp[j] - (char *)lp->d_partitions)
				    / sizeof(**spp);
				printf("\nError, partitions %c and %c overlap:\n",
				    'a' + i, 'a' + j);
				printf("#    %16.16s %16.16s  fstype "
				    "[fsize bsize  cpg]\n", "size", "offset");
				display_partition(stdout, lp, NULL, i, 0);
				display_partition(stdout, lp, NULL, j, 0);

				/* Get partition to disable or ^D */
				do {
					printf("Disable which one? (^D to abort) [%c %c] ",
					    'a' + i, 'a' + j);
					buf[0] = '\0';
					if (!fgets(buf, sizeof(buf), stdin)) {
						putchar('\n');
						return(1);	/* ^D */
					}
					c = buf[0] - 'a';
				} while (buf[1] != '\n' && buf[1] != '\0' &&
				    c != i && c != j);

				/* Mark the selected one as unused */
				lp->d_partitions[c].p_fstype = FS_UNUSED;
				return (has_overlap(lp));
			}
		}
	}

	return(0);
}

void
edit_parms(struct disklabel *lp)
{
	char *p;
	u_int64_t freesectors, ui;
	struct disklabel oldlabel = *lp;

	printf("Changing device parameters for %s:\n", specname);

	/* disk type */
	for (;;) {
		p = getstring("disk type",
		    "What kind of disk is this?  Usually SCSI, ESDI, ST506, or "
		    "floppy (use ESDI for IDE).", dktypenames[lp->d_type]);
		if (p == NULL) {
			fputs("Command aborted\n", stderr);
			return;
		}
		if (strcasecmp(p, "IDE") == 0)
			ui = DTYPE_ESDI;
		else
			for (ui = 1; ui < DKMAXTYPES &&
			    strcasecmp(p, dktypenames[ui]); ui++)
				;
		if (ui < DKMAXTYPES) {
			break;
		} else {
			printf("\"%s\" is not a valid disk type.\n", p);
			fputs("Valid types are: ", stdout);
			for (ui = 1; ui < DKMAXTYPES; ui++) {
				printf("\"%s\"", dktypenames[ui]);
				if (ui < DKMAXTYPES - 1)
					fputs(", ", stdout);
			}
			putchar('\n');
		}
	}
	lp->d_type = ui;

	/* pack/label id */
	p = getstring("label name",
	    "15 char string that describes this label, usually the disk name.",
	    lp->d_packname);
	if (p == NULL) {
		fputs("Command aborted\n", stderr);
		*lp = oldlabel;		/* undo damage */
		return;
	}
	strncpy(lp->d_packname, p, sizeof(lp->d_packname));	/* checked */

	/* sectors/track */
	for (;;) {
		ui = getuint(lp, "sectors/track",
		    "The Numer of sectors per track.", lp->d_nsectors,
		    lp->d_nsectors, 0, 0);
		if (ui == ULLONG_MAX - 1) {
			fputs("Command aborted\n", stderr);
			*lp = oldlabel;		/* undo damage */
			return;
		} if (ui == ULLONG_MAX)
			fputs("Invalid entry\n", stderr);
		else
			break;
	}
	lp->d_nsectors = ui;

	/* tracks/cylinder */
	for (;;) {
		ui = getuint(lp, "tracks/cylinder",
		    "The number of tracks per cylinder.", lp->d_ntracks,
		    lp->d_ntracks, 0, 0);
		if (ui == ULLONG_MAX - 1) {
			fputs("Command aborted\n", stderr);
			*lp = oldlabel;		/* undo damage */
			return;
		} else if (ui == ULLONG_MAX)
			fputs("Invalid entry\n", stderr);
		else
			break;
	}
	lp->d_ntracks = ui;

	/* sectors/cylinder */
	for (;;) {
		ui = getuint(lp, "sectors/cylinder",
		    "The number of sectors per cylinder (Usually sectors/track "
		    "* tracks/cylinder).", lp->d_secpercyl, lp->d_secpercyl,
		    0, 0);
		if (ui == ULLONG_MAX - 1) {
			fputs("Command aborted\n", stderr);
			*lp = oldlabel;		/* undo damage */
			return;
		} else if (ui == ULLONG_MAX)
			fputs("Invalid entry\n", stderr);
		else
			break;
	}
	lp->d_secpercyl = ui;

	/* number of cylinders */
	for (;;) {
		ui = getuint(lp, "number of cylinders",
		    "The total number of cylinders on the disk.",
		    lp->d_ncylinders, lp->d_ncylinders, 0, 0);
		if (ui == ULLONG_MAX - 1) {
			fputs("Command aborted\n", stderr);
			*lp = oldlabel;		/* undo damage */
			return;
		} else if (ui == ULLONG_MAX)
			fputs("Invalid entry\n", stderr);
		else
			break;
	}
	lp->d_ncylinders = ui;

	/* total sectors */
	for (;;) {
		u_int64_t nsec = MAX(DL_GETDSIZE(lp),
		    (u_int64_t)lp->d_ncylinders * lp->d_secpercyl);
		ui = getuint(lp, "total sectors",
		    "The total number of sectors on the disk.",
		    nsec, nsec, 0, 0);
		if (ui == ULLONG_MAX - 1) {
			fputs("Command aborted\n", stderr);
			*lp = oldlabel;		/* undo damage */
			return;
		} else if (ui == ULLONG_MAX)
			fputs("Invalid entry\n", stderr);
		else if (ui > DL_GETDSIZE(lp) &&
		    ending_sector == DL_GETDSIZE(lp)) {
			puts("You may want to increase the size of the 'c' "
			    "partition.");
			break;
		} else if (ui < DL_GETDSIZE(lp) &&
		    ending_sector == DL_GETDSIZE(lp)) {
			/* shrink free count */
			freesectors = editor_countfree(lp);
			if (DL_GETDSIZE(lp) - ui > freesectors)
				fprintf(stderr,
				    "Not enough free space to shrink by %llu "
				    "sectors (only %llu sectors left)\n",
				    DL_GETDSIZE(lp) - ui, freesectors);
			else
				break;
		} else
			break;
	}
	/* Adjust ending_sector if necessary. */
	if (ending_sector > ui)
		ending_sector = ui;
	DL_SETDSIZE(lp, ui);

	/* rpm */
	for (;;) {
		ui = getuint(lp, "rpm",
		  "The rotational speed of the disk in revolutions per minute.",
		  lp->d_rpm, lp->d_rpm, 0, 0);
		if (ui == ULLONG_MAX - 1) {
			fputs("Command aborted\n", stderr);
			*lp = oldlabel;		/* undo damage */
			return;
		} else if (ui == ULLONG_MAX)
			fputs("Invalid entry\n", stderr);
		else
			break;
	}
	lp->d_rpm = ui;

	/* interleave */
	for (;;) {
		ui = getuint(lp, "interleave",
		  "The physical sector interleave, set when formatting.  Almost always 1.",
		  lp->d_interleave, lp->d_interleave, 0, 0);
		if (ui == ULLONG_MAX - 1) {
			fputs("Command aborted\n", stderr);
			*lp = oldlabel;		/* undo damage */
			return;
		} else if (ui == ULLONG_MAX || ui == 0)
			fputs("Invalid entry\n", stderr);
		else
			break;
	}
	lp->d_interleave = ui;
}

struct partition **
sort_partitions(struct disklabel *lp)
{
	static struct partition *spp[MAXPARTITIONS+2];
	int i, npartitions;

	memset(spp, 0, sizeof(spp));

	for (npartitions = 0, i = 0; i < lp->d_npartitions; i++) {
		if (lp->d_partitions[i].p_fstype != FS_UNUSED &&
		    lp->d_partitions[i].p_fstype != FS_BOOT &&
		    DL_GETPSIZE(&lp->d_partitions[i]) != 0)
			spp[npartitions++] = &lp->d_partitions[i];
	}

	/*
	 * Sort the partitions based on starting offset.
	 * This is safe because we guarantee no overlap.
	 */
	if (npartitions > 1)
		if (heapsort((void *)spp, npartitions, sizeof(spp[0]),
		    partition_cmp))
			err(4, "failed to sort partition table");

	return(spp);
}

/*
 * Get a valid disk type if necessary.
 */
void
getdisktype(struct disklabel *lp, char *banner, char *dev)
{
	int i;
	char *s, *def = "SCSI";
	struct dtypes {
		char *dev;
		char *type;
	} dtypes[] = {
		{ "sd",   "SCSI" },
		{ "rz",   "SCSI" },
		{ "wd",   "IDE" },
		{ "fd",   "FLOPPY" },
		{ "xd",   "SMD" },
		{ "xy",   "SMD" },
		{ "hd",   "HP-IB" },
		{ "ccd",  "CCD" },
		{ "vnd",  "VND" },
		{ "svnd", "VND" },
		{ NULL,   NULL }
	};

	if ((s = basename(dev)) != NULL) {
		if (*s == 'r')
			s++;
		i = strcspn(s, "0123456789");
		s[i] = '\0';
		dev = s;
		for (i = 0; dtypes[i].dev != NULL; i++) {
			if (strcmp(dev, dtypes[i].dev) == 0) {
				def = dtypes[i].type;
				break;
			}
		}
	}

	if (lp->d_type > DKMAXTYPES || lp->d_type == 0) {
		puts(banner);
		puts("Possible values are:");
		printf("\"IDE\", ");
		for (i = 1; i < DKMAXTYPES; i++) {
			printf("\"%s\"", dktypenames[i]);
			if (i < DKMAXTYPES - 1)
				fputs(", ", stdout);
		}
		putchar('\n');

		for (;;) {
			s = getstring("Disk type",
			    "What kind of disk is this?  Usually SCSI, IDE, "
			    "ESDI, CCD, ST506, or floppy.", def);
			if (s == NULL)
				continue;
			if (strcasecmp(s, "IDE") == 0) {
				lp->d_type = DTYPE_ESDI;
				return;
			}
			for (i = 1; i < DKMAXTYPES; i++)
				if (strcasecmp(s, dktypenames[i]) == 0) {
					lp->d_type = i;
					return;
				}
			printf("\"%s\" is not a valid disk type.\n", s);
			fputs("Valid types are: ", stdout);
			for (i = 1; i < DKMAXTYPES; i++) {
				printf("\"%s\"", dktypenames[i]);
				if (i < DKMAXTYPES - 1)
					fputs(", ", stdout);
			}
			putchar('\n');
		}
	}
}

/*
 * Get beginning and ending sectors of the OpenBSD portion of the disk
 * from the user.
 * XXX - should mention MBR values if DOSLABEL
 */
void
set_bounds(struct disklabel *lp)
{
	u_int64_t ui, start_temp;

	/* Starting sector */
	do {
		ui = getuint(lp, "Starting sector",
		  "The start of the OpenBSD portion of the disk.",
		  starting_sector, DL_GETDSIZE(lp), 0, 0);
		if (ui == ULLONG_MAX - 1) {
			fputs("Command aborted\n", stderr);
			return;
		}
	} while (ui >= DL_GETDSIZE(lp));
	start_temp = ui;

	/* Size */
	do {
		ui = getuint(lp, "Size ('*' for entire disk)",
		  "The size of the OpenBSD portion of the disk ('*' for the "
		  "entire disk).", ending_sector - starting_sector,
		  DL_GETDSIZE(lp) - start_temp, 0, 0);
		if (ui == ULLONG_MAX - 1) {
			fputs("Command aborted\n", stderr);
			return;
		}
	} while (ui > DL_GETDSIZE(lp) - start_temp);
	ending_sector = start_temp + ui;
	starting_sector = start_temp;
}

/*
 * Return a list of the "chunks" of free space available
 */
struct diskchunk *
free_chunks(struct disklabel *lp)
{
	struct partition **spp;
	static struct diskchunk chunks[MAXPARTITIONS + 2];
	u_int64_t start, stop;
	int i, numchunks;

	/* Sort the in-use partitions based on offset */
	spp = sort_partitions(lp);

	/* If there are no partitions, it's all free. */
	if (spp[0] == NULL) {
		chunks[0].start = starting_sector;
		chunks[0].stop = ending_sector;
		chunks[1].start = chunks[1].stop = 0;
		return(chunks);
	}

	/* Find chunks of free space */
	numchunks = 0;
	if (DL_GETPOFFSET(spp[0]) > starting_sector) {
		chunks[0].start = starting_sector;
		chunks[0].stop = DL_GETPOFFSET(spp[0]);
		numchunks++;
	}
	for (i = 0; spp[i] != NULL; i++) {
		start = DL_GETPOFFSET(spp[i]) + DL_GETPSIZE(spp[i]);
		if (start < starting_sector)
			start = starting_sector;
		else if (start > ending_sector)
			start = ending_sector;
		if (spp[i + 1] != NULL)
			stop = DL_GETPOFFSET(spp[i+1]);
		else
			stop = ending_sector;
		if (stop < starting_sector)
			stop = starting_sector;
		else if (stop > ending_sector)
			stop = ending_sector;
		if (start < stop) {
			chunks[numchunks].start = start;
			chunks[numchunks].stop = stop;
			numchunks++;
		}
	}

	/* Terminate and return */
	chunks[numchunks].start = chunks[numchunks].stop = 0;
	return(chunks);
}

/*
 * What is the OpenBSD portion of the disk?  Uses the MBR if applicable.
 */
void
find_bounds(struct disklabel *lp)
{
#ifdef DOSLABEL
	struct partition *pp = &lp->d_partitions[RAW_PART];
	u_int64_t new_end;
	int i;
#endif
	/* Defaults */
	/* XXX - reserve a cylinder for hp300? */
	starting_sector = 0;
	ending_sector = DL_GETDSIZE(lp);

#ifdef DOSLABEL
	/*
	 * If we have an MBR, use values from the OpenBSD partition.
	 */
	if (dosdp) {
	    if (dosdp->dp_typ == DOSPTYP_OPENBSD) {
			/* Set start and end based on fdisk partition bounds */
			starting_sector = letoh32(dosdp->dp_start);
			ending_sector = starting_sector + letoh32(dosdp->dp_size);

			/*
			 * If there are any BSD or SWAP partitions beyond
			 * ending_sector we extend ending_sector to include
			 * them.  This is done because the BIOS geometry is
			 * generally different from the disk geometry.
			 */
			for (i = new_end = 0; i < lp->d_npartitions; i++) {
				pp = &lp->d_partitions[i];
				if ((pp->p_fstype == FS_BSDFFS ||
				    pp->p_fstype == FS_SWAP) &&
				    DL_GETPSIZE(pp) + DL_GETPOFFSET(pp) >
					new_end)
					new_end = DL_GETPSIZE(pp) +
					    DL_GETPOFFSET(pp);
			}
			if (new_end > ending_sector)
				ending_sector = new_end;
		} else {
			/* Don't trounce the MBR */
			starting_sector = 63;
		}

		printf("Treating sectors %llu-%llu as the OpenBSD portion of the "
		    "disk.\nYou can use the 'b' command to change this.\n\n",
		    starting_sector, ending_sector);
	}
#elif (NUMBOOT == 1)
	/* Boot blocks take up the first cylinder */
	starting_sector = lp->d_secpercyl;
	printf("Reserving the first data cylinder for boot blocks.\n"
	    "You can use the 'b' command to change this.\n\n");
#endif
}

/*
 * Calculate free space.
 */
u_int64_t
editor_countfree(struct disklabel *lp)
{
	struct diskchunk *chunks;
	u_int64_t freesectors = 0;
	int i;

	chunks = free_chunks(lp);

	for (i = 0; chunks[i].start != 0 || chunks[i].stop != 0; i++)
		freesectors += chunks[i].stop - chunks[i].start;

	return (freesectors);	
}

void
editor_help(char *arg)
{

	/* XXX - put these strings in a table instead? */
	switch (*arg) {
	case 'p':
		puts(
"The 'p' command prints the current partitions.  By default, it prints size\n"
"and offset in sectors (a sector is usually 512 bytes).  The 'p' command\n"
"takes an optional units argument.  Possible values are 'b' for bytes, 'c'\n"
"for cylinders, 'k' for kilobytes, 'm' for megabytes, and 'g' for gigabytes.\n");
	case 'l':
	puts(
"The 'l' command prints the header of the disk label.  By default, it prints\n"
"size and offset in sectors (a sector is usually 512 bytes).  The 'p' command\n"
"takes an optional units argument.  Possible values are 'b' for bytes, 'c'\n"
"for cylinders, 'k' for kilobytes, 'm' for megabytes, and 'g' for gigabytes.\n");
		break;
	case 'M':
		puts(
"The 'M' command pipes the entire OpenBSD manual page for disk label through\n"
"the pager specified by the PAGER environment variable or 'less' if PAGER is\n"
"not set.  It is especially useful during install when the normal system\n"
"manual is not available.\n");
		break;
	case 'e':
		puts(
"The 'e' command is used to edit the disk drive parameters.  These include\n"
"the number of sectors/track, tracks/cylinder, sectors/cylinder, number of\n"
"cylinders on the disk , total sectors on the disk, rpm, interleave, disk\n"
"type, and a descriptive label string.  You should not change these unless\n"
"you know what you are doing\n");
		break;
	case 'a':
		puts(
"The 'a' command adds new partitions to the disk.  It takes as an optional\n"
"argument the partition letter to add.  If you do not specify a partition\n"
"letter, you will be prompted for it; the next available letter will be the\n"
"default answer\n");
		break;
	case 'b':
		puts(
"The 'b' command is used to change the boundaries of the OpenBSD portion of\n"
"the disk.  This is only useful on disks with an fdisk partition.  By default,\n"
"on a disk with an fdisk partition, the boundaries are set to be the first\n"
"and last sectors of the OpenBSD fdisk partition.  You should only change\n"
"these if your fdisk partition table is incorrect or you have a disk larger\n"
"than 8gig, since 8gig is the maximum size an fdisk partition can be.  You\n"
"may enter '*' at the 'Size' prompt to indicate the entire size of the disk\n"
"(minus the starting sector).  Use this option with care; if you extend the\n"
"boundaries such that they overlap with another operating system you will\n"
"corrupt the other operating system's data.\n");
		break;
	case 'c':
		puts(
"The 'c' command is used to change the size of an existing partition.  It\n"
"takes as an optional argument the partition letter to change.  If you do not\n"
"specify a partition letter, you will be prompted for one.  You may add a '+'\n"
"or '-' prefix to the new size to increase or decrease the existing value\n"
"instead of entering an absolute value.  You may also use a suffix to indicate\n"
"the units the values is in terms of.  Possible suffixes are 'b' for bytes,\n"
"'c' for cylinders, 'k' for kilobytes, 'm' for megabytes, 'g' for gigabytes or\n"
"no suffix for sectors (usually 512 bytes).  You may also enter '*' to change\n"
"the size to be the total number of free sectors remaining.\n");
		break;
	case 'D':
		puts(
"The 'D' command will set the disk label to the default values as reported\n"
"by the disk itself.  This similates the case where there is no disk label.\n");
		break;
	case 'd':
		puts(
"The 'd' command is used to delete an existing partition.  It takes as an\n"
"optional argument the partition letter to change.  If you do not specify a\n"
"partition letter, you will be prompted for one.  You may not delete the ``c''\n"
"partition as 'c' must always exist and by default is marked as 'unused' (so\n"
"it does not take up any space).\n");
		break;
	case 'g':
		puts(
"The 'g' command is used select which disk geometry to use, the disk or a\n"
"user geometry.  It takes as an optional argument ``d'' or ``u''.  If \n"
"you do not specify the type as an argument, you will be prompted for it.\n");
		break;
	case 'm':
		puts(
"The 'm' command is used to modify an existing partition.  It takes as an\n"
"optional argument the partition letter to change.  If you do not specify a\n"
"partition letter, you will be prompted for one.  This option allows the user\n"
"to change the filesystem type, starting offset, partition size, block fragment\n"
"size, block size, and cylinders per group for the specified partition (not all\n"
"parameters are configurable for non-BSD partitions).\n");
		break;
	case 'n':
		puts(
"The 'n' command is used to set the mount point for a partition (ie: name it).\n"
"It takes as an optional argument the partition letter to name.  If you do\n"
"not specify a partition letter, you will be prompted for one.  This option\n"
"is only valid if disklabel was invoked with the -F flag.\n");
		break;
	case 'r':
		puts(
"The 'r' command is used to recalculate and display details about\n"
"the available free space.\n");
		break;
	case 'u':
		puts(
"The 'u' command will undo (or redo) the last change.  Entering 'u' once will\n"
"undo your last change.  Entering it again will restore the change.\n");
		break;
	case 's':
		puts(
"The 's' command is used to save a copy of the label to a file in ascii format\n"
"(suitable for loading via disklabel's [-R] option).  It takes as an optional\n"
"argument the filename to save the label to.  If you do not specify a filename,\n"
"you will be prompted for one.\n");
		break;
	case 'w':
		puts(
"The 'w' command will write the current label to disk.  This option will\n"
"commit any changes to the on-disk label.\n");
		break;
	case 'q':
		puts(
"The 'q' command quits the label editor.  If any changes have been made you\n"
"will be asked whether or not to save the changes to the on-disk label.\n");
		break;
	case 'X':
		puts(
"The 'X' command toggles disklabel in to/out of 'expert mode'.  By default,\n"
"some settings are reserved for experts only (such as the block and fragment\n"
"size on ffs partitions).\n");
		break;
	case 'x':
		puts(
"The 'x' command exits the label editor without saving any changes to the\n"
"on-disk label.\n");
		break;
	case 'z':
		puts(
"The 'z' command zeroes out the existing partition table, leaving only the 'c'\n"
"partition.  The drive parameters are not changed.\n");
		break;
	default:
		puts("Available commands:");
		puts(
"  ? [command] - show help                   n [part] - set mount point\n"
"  a [part]    - add partition               p [unit] - print partitions\n"
"  b           - set OpenBSD boundaries      q        - quit & save changes\n"
"  c [part]    - change partition size       r        - display free space\n"
"  D           - reset label to default      s [path] - save label to file\n"
"  d [part]    - delete partition            u        - undo last change\n"
"  e           - edit drive parameters       w        - write label to disk\n"
"  g [d|u]     - [d]isk or [u]ser geometry   X        - toggle expert mode\n"
"  l [unit]    - print disk label header     x        - exit w/o saving changes\n"
"  M           - disklabel(8) man page       z        - delete all partitions\n"
"  m [part]    - modify partition\n"
"\n"
"Suffixes can be used to indicate units other than sectors:\n"
"\t'b' (bytes), 'k' (kilobytes), 'm' (megabytes), 'g' (gigabytes)\n"
"\t'c' (cylinders), '%' (% of total disk), '&' (% of free space).\n"
"Values in non-sector units are truncated to the nearest cylinder boundary.");
		break;
	}
}

char **
mpcopy(char **to, char **from)
{
	int i;
	char *top;

	for (i = 0; i < MAXPARTITIONS; i++) {
		if (from[i] != NULL) {
			int len = strlen(from[i]) + 1;

			top = realloc(to[i], len);
			if (top == NULL)
				errx(4, "out of memory");
			to[i] = top;
			(void)strlcpy(to[i], from[i], len);
		} else if (to[i] != NULL) {
			free(to[i]);
			to[i] = NULL;
		}
	}
	return(to);
}

int
mpequal(char **mp1, char **mp2)
{
	int i;

	for (i = 0; i < MAXPARTITIONS; i++) {
		if (mp1[i] == NULL && mp2[i] == NULL)
			continue;

		if ((mp1[i] != NULL && mp2[i] == NULL) ||
		    (mp1[i] == NULL && mp2[i] != NULL) ||
		    (strcmp(mp1[i], mp2[i]) != 0))
			return(0);
	}
	return(1);
}

int
mpsave(struct disklabel *lp, char **mp, char *cdev, char *fstabfile)
{
	int i, j, mpset;
	char bdev[MAXPATHLEN], *p;
	struct mountinfo mi[MAXPARTITIONS];
	FILE *fp;

	memset(&mi, 0, sizeof(mi));

	for (i = 0, mpset = 0; i < MAXPARTITIONS; i++) {
		if (mp[i] != NULL) {
			mi[i].mountpoint = mp[i];
			mi[i].partno = i;
			mpset = 1;
		}
	}
	/* Exit if there is nothing to do... */
	if (!mpset)
		return(0);

	/* Convert cdev to bdev */
	if (strncmp(_PATH_DEV, cdev, sizeof(_PATH_DEV) - 1) == 0 &&
	    cdev[sizeof(_PATH_DEV) - 1] == 'r') {
		snprintf(bdev, sizeof(bdev), "%s%s", _PATH_DEV,
		    &cdev[sizeof(_PATH_DEV)]);
	} else {
		if ((p = strrchr(cdev, '/')) == NULL || *(++p) != 'r')
			return(1);
		*p = '\0';
		snprintf(bdev, sizeof(bdev), "%s%s", cdev, p + 1);
		*p = 'r';
	}
	bdev[strlen(bdev) - 1] = '\0';

	/* Sort mountpoints so we don't try to mount /usr/local before /usr */
	qsort((void *)mi, MAXPARTITIONS, sizeof(struct mountinfo), micmp);

	if ((fp = fopen(fstabfile, "w")) == NULL)
		return(1);

	for (i = 0; i < MAXPARTITIONS && mi[i].mountpoint != NULL; i++) {
		j =  mi[i].partno;
		fprintf(fp, "%s%c %s %s rw 1 %d\n", bdev, 'a' + j,
		    mi[i].mountpoint,
		    fstypesnames[lp->d_partitions[j].p_fstype],
		    j == 0 ? 1 : 2);
	}
	fclose(fp);
	return(0);
}

int
get_offset(struct disklabel *lp, int partno)
{
	struct diskchunk *chunks;
	struct partition *pp = &lp->d_partitions[partno];
	u_int64_t ui, maxsize;
	int i, fstype;

	ui = getuint(lp, "offset",
	   "Starting sector for this partition.",
	   DL_GETPOFFSET(pp),
	   DL_GETPOFFSET(pp), 0, DO_CONVERSIONS |
	   (pp->p_fstype == FS_BSDFFS ? DO_ROUNDING : 0));

	if (ui == ULLONG_MAX - 1)
		fputs("Command aborted\n", stderr);
	else if (ui == ULLONG_MAX)
		fputs("Invalid entry\n", stderr);
	else if (ui < starting_sector || ui >= ending_sector)
		fprintf(stderr, "The offset must be >= %llu and < %llu, "
		    "the limits of the OpenBSD portion\n"
		    "of the disk. The 'b' command can change these limits.\n",
		    starting_sector, ending_sector);
#ifdef SUN_AAT0
	else if (partno == 0 && ui != 0)
		fprintf(stderr, "This architecture requires that "
		    "partition 'a' start at sector 0.\n");
#endif
	else {
		fstype = pp->p_fstype;
		pp->p_fstype = FS_UNUSED;
		chunks = free_chunks(lp);
		pp->p_fstype = fstype;
		for (i = 0; chunks[i].start != 0 || chunks[i].stop != 0; i++) {
			if (ui < chunks[i].start || ui >= chunks[i].stop)
				continue;
			DL_SETPOFFSET(pp, ui);
			maxsize = chunks[i].stop - DL_GETPOFFSET(pp);
			if (DL_GETPSIZE(pp) > maxsize)
				DL_SETPSIZE(pp, maxsize);
			return (0);
		}
		fputs("The offset must be in a free area.\n", stderr);
	}

	/* Partition offset was not set. */
	return (1);
}

int
get_size(struct disklabel *lp, int partno)
{
	struct partition *pp = &lp->d_partitions[partno];
	u_int64_t maxsize, ui;

	maxsize = max_partition_size(lp, partno);

	ui = getuint(lp, "size", "Size of the partition. "
	    "You may also say +/- amount for a relative change.",
	    DL_GETPSIZE(pp), maxsize, DL_GETPOFFSET(pp),
	    DO_CONVERSIONS | ((pp->p_fstype == FS_BSDFFS ||
	    pp->p_fstype == FS_SWAP) ?  DO_ROUNDING : 0));

	if (ui == ULLONG_MAX - 1)
		fputs("Command aborted\n", stderr);
	else if (ui == ULLONG_MAX)
		fputs("Invalid entry\n", stderr);
	else if (ui == 0)
		fputs("The size must be > 0\n", stderr);
	else if (ui + DL_GETPOFFSET(pp) > ending_sector)
		fprintf(stderr, "The size can't be more than "
		    "%llu sectors, or the partition would\n"
		    "extend beyond the last sector (%llu) of the "
		    "OpenBSD portion of\nthe disk. "
		    "The 'b' command can change this limit.\n",
		    ending_sector - DL_GETPOFFSET(pp), ending_sector);
	else if (ui > maxsize)
		fprintf(stderr,"Sorry, there are only %llu sectors left\n",
		    maxsize);
	else {
		DL_SETPSIZE(pp, ui);
		return (0);
	}

	/* Partition size was not set. */
	return (1);
}

int
get_fsize(struct disklabel *lp, int partno)
{
	u_int64_t ui, fsize, frag;
	struct partition *pp = &lp->d_partitions[partno];
	
	if (!expert || pp->p_fstype != FS_BSDFFS)
		return (0);

	fsize = DISKLABELV1_FFS_FSIZE(pp->p_fragblock);
	frag = DISKLABELV1_FFS_FRAG(pp->p_fragblock);
	if (fsize == 0)
		frag = 8;

	for (;;) {
		ui = getuint(lp, "fragment size",
		    "Size of fs block fragments.  Usually 2048 or 512.",
		    fsize, fsize, 0, 0);
		if (ui == ULLONG_MAX - 1) {
			fputs("Command aborted\n", stderr);
			return(1);
		} else if (ui == ULLONG_MAX)
			fputs("Invalid entry\n", stderr);
		else
			break;
	}
	if (ui == 0)
		puts("Zero fragment size implies zero block size");
	pp->p_fragblock = DISKLABELV1_FFS_FRAGBLOCK(ui, frag);
	return(0);
}

int
get_bsize(struct disklabel *lp, int partno)
{
	u_int64_t ui, bsize, frag, fsize;
	struct partition *pp = &lp->d_partitions[partno];

	if (!expert || pp->p_fstype != FS_BSDFFS)
		return (0);

	/* Avoid dividing by zero... */
	if (pp->p_fragblock == 0)
		return(1);

	bsize = DISKLABELV1_FFS_BSIZE(pp->p_fragblock);
	fsize = DISKLABELV1_FFS_FSIZE(pp->p_fragblock);
	frag = DISKLABELV1_FFS_FRAG(pp->p_fragblock);

	for (;;) {
		ui = getuint(lp, "block size",
		    "Size of filesystem blocks.  Usually 16384 or 4096.",
		    fsize * frag, fsize * frag,
		    0, 0);

		/* sanity checks */
		if (ui == ULLONG_MAX - 1) {
			fputs("Command aborted\n", stderr);
			return(1);
		} else if (ui == ULLONG_MAX)
			fputs("Invalid entry\n", stderr);
		else if (ui < getpagesize())
			fprintf(stderr,
			    "Error: block size must be at least as big "
			    "as page size (%d).\n", getpagesize());
		else if (ui % fsize != 0)
			fputs("Error: block size must be a multiple of the "
			    "fragment size.\n", stderr);
		else if (ui / fsize < 1)
			fputs("Error: block size must be at least as big as "
			    "fragment size.\n", stderr);
		else
			break;
	}
	pp->p_fragblock = DISKLABELV1_FFS_FRAGBLOCK(ui / frag, frag);
	return(0);
}

int
get_fstype(struct disklabel *lp, int partno)
{
	char *p;
	u_int64_t ui;
	struct partition *pp = &lp->d_partitions[partno];

	if (pp->p_fstype < FSMAXTYPES) {
		p = getstring("FS type",
		    "Filesystem type (usually 4.2BSD or swap)",
		    fstypenames[pp->p_fstype]);
		if (p == NULL) {
			fputs("Command aborted\n", stderr);
			return(1);
		}
		for (ui = 0; ui < FSMAXTYPES; ui++) {
			if (!strcasecmp(p, fstypenames[ui])) {
				pp->p_fstype = ui;
				break;
			}
		}
		if (ui >= FSMAXTYPES) {
			printf("Unrecognized filesystem type '%s', treating as 'unknown'\n", p);
			pp->p_fstype = FS_OTHER;
		}
	} else {
		for (;;) {
			ui = getuint(lp, "FS type (decimal)",
			    "Filesystem type as a decimal number; usually 7 (4.2BSD) or 1 (swap).",
			    pp->p_fstype, pp->p_fstype, 0, 0);
			if (ui == ULLONG_MAX - 1) {
				fputs("Command aborted\n", stderr);
				return(1);
			} if (ui == ULLONG_MAX)
				fputs("Invalid entry\n", stderr);
			else
				break;
		}
		pp->p_fstype = ui;
	}
	return(0);
}

int
get_mp(struct disklabel *lp, char **mp, int partno)
{
	char *p;
	struct partition *pp = &lp->d_partitions[partno];

	if (mp != NULL && pp->p_fstype != FS_UNUSED &&
	    pp->p_fstype != FS_SWAP && pp->p_fstype != FS_BOOT &&
	    pp->p_fstype != FS_OTHER) {
		for (;;) {
			p = getstring("mount point",
			    "Where to mount this filesystem (ie: / /var /usr)",
			    mp[partno] ? mp[partno] : "none");
			if (p == NULL) {
				fputs("Command aborted\n", stderr);
				return(1);
			}
			if (strcasecmp(p, "none") == 0) {
				if (mp[partno] != NULL) {
					free(mp[partno]);
					mp[partno] = NULL;
				}
				break;
			}
			if (*p == '/') {
				/* XXX - might as well realloc */
				if (mp[partno] != NULL)
					free(mp[partno]);
				if ((mp[partno] = strdup(p)) == NULL)
					errx(4, "out of memory");
				break;
			}
			fputs("Mount points must start with '/'\n", stderr);
		}
	}
	return(0);
}

int
micmp(const void *a1, const void *a2)
{
	struct mountinfo *mi1 = (struct mountinfo *)a1;
	struct mountinfo *mi2 = (struct mountinfo *)a2;

	/* We want all the NULLs at the end... */
	if (mi1->mountpoint == NULL && mi2->mountpoint == NULL)
		return(0);
	else if (mi1->mountpoint == NULL)
		return(1);
	else if (mi2->mountpoint == NULL)
		return(-1);
	else
		return(strcmp(mi1->mountpoint, mi2->mountpoint));
}

void
get_geometry(int f, struct disklabel **dgpp)
{
	struct stat st;
	struct disklabel *disk_geop;

	if (fstat(f, &st) == -1)
		err(4, "Can't stat device");

	/* Get disk geometry */
	if ((disk_geop = calloc(1, sizeof(struct disklabel))) == NULL)
		errx(4, "out of memory");
	if (ioctl(f, DIOCGPDINFO, disk_geop) < 0 &&
	    ioctl(f, DIOCGDINFO, disk_geop) < 0)
		err(4, "ioctl DIOCGDINFO");
	*dgpp = disk_geop;
}

void
set_geometry(struct disklabel *lp, struct disklabel *dgp,
    struct disklabel *ugp, char *p)
{
	if (p == NULL) {
		p = getstring("[d]isk or [u]ser geometry",
		    "Enter 'd' to use the geometry based on what the disk "
		    "itself thinks it is, or 'u' to use the geometry that "
		    "was found in the label.",
		    "d");
	}
	if (p == NULL) {
		fputs("Command aborted\n", stderr);
		return;
	}
	switch (*p) {
	case 'd':
	case 'D':
		if (dgp == NULL)
			fputs("BIOS geometry not defined.\n", stderr);
		else {
			lp->d_secsize = dgp->d_secsize;
			lp->d_nsectors = dgp->d_nsectors;
			lp->d_ntracks = dgp->d_ntracks;
			lp->d_ncylinders = dgp->d_ncylinders;
			lp->d_secpercyl = dgp->d_secpercyl;
			DL_SETDSIZE(lp, DL_GETDSIZE(dgp));
		}
		break;
	case 'u':
	case 'U':
		if (ugp == NULL)
			fputs("BIOS geometry not defined.\n", stderr);
		else {
			lp->d_secsize = ugp->d_secsize;
			lp->d_nsectors = ugp->d_nsectors;
			lp->d_ntracks = ugp->d_ntracks;
			lp->d_ncylinders = ugp->d_ncylinders;
			lp->d_secpercyl = ugp->d_secpercyl;
			DL_SETDSIZE(lp, DL_GETDSIZE(ugp));
			if (dgp != NULL && ugp->d_secsize == dgp->d_secsize &&
			    ugp->d_nsectors == dgp->d_nsectors &&
			    ugp->d_ntracks == dgp->d_ntracks &&
			    ugp->d_ncylinders == dgp->d_ncylinders &&
			    ugp->d_secpercyl == dgp->d_secpercyl &&
			    DL_GETDSIZE(ugp) == DL_GETDSIZE(dgp))
				fputs("Note: user geometry is the same as disk "
				    "geometry.\n", stderr);
		}
		break;
	default:
		fputs("You must enter either 'd' or 'u'.\n", stderr);
		break;
	}
}

void
zero_partitions(struct disklabel *lp)
{
	int i;

	for (i = 0; i < MAXPARTITIONS; i++)
		memset(&lp->d_partitions[i], 0, sizeof(struct partition));
	DL_SETPSIZE(&lp->d_partitions[RAW_PART], DL_GETDSIZE(lp));
}

u_int64_t
max_partition_size(struct disklabel *lp, int partno)
{
	struct partition *pp = &lp->d_partitions[partno];
	struct diskchunk *chunks;
	u_int64_t maxsize, offset;
	int fstype, i;

	fstype = pp->p_fstype;
	pp->p_fstype = FS_UNUSED;
	chunks = free_chunks(lp);
	pp->p_fstype = fstype;

	offset = DL_GETPOFFSET(pp);
	for (i = 0; chunks[i].start != 0 || chunks[i].stop != 0; i++) {
		if (offset < chunks[i].start || offset >= chunks[i].stop)
			continue;
		maxsize = chunks[i].stop - offset;
		break;
	}	
	return (maxsize);
}

void
psize(daddr64_t sz, char unit, struct disklabel *lp)
{
	double d = scale(sz, unit, lp);
	if (d < 0)
		printf("%llu", sz);
	else
		printf("%.*f%c", unit == 'B' ? 0 : 1, d, unit);
}

void
display_edit(struct disklabel *lp, char **mp, char unit, u_int64_t fr)
{
	int i;

	unit = toupper(unit);

	printf("OpenBSD area: ");
	psize(starting_sector, unit, lp);
	printf("-");
	psize(ending_sector, unit, lp);
	printf("; size: ");
	psize(ending_sector - starting_sector, unit, lp);
	printf("; free: ");
	psize(fr, unit, lp);

	printf("\n#    %16.16s %16.16s  fstype [fsize bsize  cpg]\n",
	    "size", "offset");
	for (i = 0; i < lp->d_npartitions; i++)
		display_partition(stdout, lp, mp, i, unit);
}

