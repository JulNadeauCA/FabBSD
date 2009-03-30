/*	$OpenBSD: fileio.c,v 1.81 2007/11/27 16:22:14 martynas Exp $	*/

/* This file is in the public domain. */

/*
 *	POSIX fileio.c
 */
#include "def.h"


#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <limits.h>
#include <dirent.h>
#include <pwd.h>
#include <string.h>
#include <unistd.h>

#include "kbd.h"

static FILE	*ffp;

/*
 * Open a file for reading.
 */
int
ffropen(const char *fn, struct buffer *bp)
{
	struct stat	statbuf;

	if ((ffp = fopen(fn, "r")) == NULL) {
		if (errno == ENOENT)
			return (FIOFNF);
		return (FIOERR);
	}

	/* If 'fn' is a directory open it with dired. */
	if (fisdir(fn) == TRUE)
		return (FIODIR);

	if (bp && fstat(fileno(ffp), &statbuf) == 0) {
		/* set highorder bit to make sure this isn't all zero */
		bp->b_fi.fi_mode = statbuf.st_mode | 0x8000;
		bp->b_fi.fi_uid = statbuf.st_uid;
		bp->b_fi.fi_gid = statbuf.st_gid;
	}

	return (FIOSUC);
}

/*
 * Open a file for writing.
 */
int
ffwopen(const char *fn, struct buffer *bp)
{
	int	fd;
	mode_t	fmode = DEFFILEMODE;

	if (bp && bp->b_fi.fi_mode)
		fmode = bp->b_fi.fi_mode & 07777;

	fd = open(fn, O_RDWR | O_CREAT | O_TRUNC, fmode);
	if (fd == -1) {
		ffp = NULL;
		ewprintf("Cannot open file for writing : %s", strerror(errno));
		return (FIOERR);
	}

	if ((ffp = fdopen(fd, "w")) == NULL) {
		ewprintf("Cannot open file for writing : %s", strerror(errno));
		close(fd);
		return (FIOERR);
	}

	/*
	 * If we have file information, use it.  We don't bother to check for
	 * errors, because there's no a lot we can do about it.  Certainly
	 * trying to change ownership will fail if we aren't root.  That's
	 * probably OK.  If we don't have info, no need to get it, since any
	 * future writes will do the same thing.
	 */
	if (bp && bp->b_fi.fi_mode) {
		fchmod(fd, bp->b_fi.fi_mode & 07777);
		fchown(fd, bp->b_fi.fi_uid, bp->b_fi.fi_gid);
	}
	return (FIOSUC);
}

/*
 * Close a file.
 * XXX - Should look at the status.
 */
/* ARGSUSED */
int
ffclose(struct buffer *bp)
{
	(void) fclose(ffp);
	return (FIOSUC);
}

/*
 * Write a buffer to the already opened file. bp points to the
 * buffer. Return the status.
 */
int
ffputbuf(struct buffer *bp)
{
	struct line   *lp, *lpend;

	lpend = bp->b_headp;
	for (lp = lforw(lpend); lp != lpend; lp = lforw(lp)) {
		if (fwrite(ltext(lp), 1, llength(lp), ffp) != llength(lp)) {
			ewprintf("Write I/O error");
			return (FIOERR);
		}
		if (lforw(lp) != lpend)		/* no implied \n on last line */
			putc('\n', ffp);
	}
	/*
	 * XXX should be variable controlled (once we have variables)
	 */
	if (llength(lback(lpend)) != 0) {
		if (eyorn("No newline at end of file, add one") == TRUE) {
			lnewline_at(lback(lpend), llength(lback(lpend)));
			putc('\n', ffp);
		}
	}
	return (FIOSUC);
}

/*
 * Read a line from a file, and store the bytes
 * in the supplied buffer. Stop on end of file or end of
 * line.  When FIOEOF is returned, there is a valid line
 * of data without the normally implied \n.
 * If the line length exceeds nbuf, FIOLONG is returned.
 */
int
ffgetline(char *buf, int nbuf, int *nbytes)
{
	int	c, i;

	i = 0;
	while ((c = getc(ffp)) != EOF && c != '\n') {
		buf[i++] = c;
		if (i >= nbuf)
			return (FIOLONG);
	}
	if (c == EOF && ferror(ffp) != FALSE) {
		ewprintf("File read error");
		return (FIOERR);
	}
	*nbytes = i;
	return (c == EOF ? FIOEOF : FIOSUC);
}

/*
 * Make a backup copy of "fname".  On Unix the backup has the same
 * name as the original file, with a "~" on the end; this seems to
 * be newest of the new-speak. The error handling is all in "file.c".
 * We do a copy instead of a rename since otherwise another process
 * with an open fd will get the backup, not the new file.  This is
 * a problem when using mg with things like crontab and vipw.
 */
int
fbackupfile(const char *fn)
{
	struct stat	 sb;
	int		 from, to, serrno;
	ssize_t		 nread;
	char		 buf[BUFSIZ];
	char		*nname, *tname;

	if (stat(fn, &sb) == -1) {
		ewprintf("Can't stat %s : %s", fn, strerror(errno));
		return (FALSE);
	}

	if (asprintf(&nname, "%s~", fn) == -1) {
		ewprintf("Can't allocate temp file name : %s", strerror(errno));
		return (ABORT);
	}

	if (asprintf(&tname, "%s.XXXXXXXXXX", fn) == -1) {
		ewprintf("Can't allocate temp file name : %s", strerror(errno));
		free(nname);
		return (ABORT);
	}

	if ((from = open(fn, O_RDONLY)) == -1) {
		free(nname);
		free(tname);
		return (FALSE);
	}
	to = mkstemp(tname);
	if (to == -1) {
		serrno = errno;
		close(from);
		free(nname);
		free(tname);
		errno = serrno;
		return (FALSE);
	}
	while ((nread = read(from, buf, sizeof(buf))) > 0) {
		if (write(to, buf, (size_t)nread) != nread) {
			nread = -1;
			break;
		}
	}
	serrno = errno;
	(void) fchmod(to, (sb.st_mode & 0777));
	close(from);
	close(to);
	if (nread == -1) {
		if (unlink(tname) == -1)
			ewprintf("Can't unlink temp : %s", strerror(errno));
	} else {
		if (rename(tname, nname) == -1) {
			ewprintf("Can't rename temp : %s", strerror(errno));
			(void) unlink(tname);
			nread = -1;
		}
	}
	free(nname);
	free(tname);
	errno = serrno;

	return (nread == -1 ? FALSE : TRUE);
}

/*
 * Convert "fn" to a canonicalized absolute filename, replacing
 * a leading ~/ with the user's home dir, following symlinks, and
 * and remove all occurrences of /./ and /../
 */
char *
adjustname(const char *fn, int slashslash)
{
	static char	 fnb[MAXPATHLEN];
	const char	*cp, *ep = NULL;
	char		 user[LOGIN_NAME_MAX], path[MAXPATHLEN];
	size_t		 ulen, plen;

	path[0] = '\0';

	if (slashslash == TRUE) {
		cp = fn + strlen(fn) - 1;
		for (; cp >= fn; cp--) {
			if (ep && (*cp == '/')) {
				fn = ep;
				break;
			}
			if (*cp == '/' || *cp == '~')
				ep = cp;
			else
				ep = NULL;
		}
	}

	/* first handle tilde expansion */
	if (fn[0] == '~') {
		struct passwd *pw;

		cp = strchr(fn, '/');
		if (cp == NULL)
			cp = fn + strlen(fn); /* point to the NUL byte */
		ulen = cp - &fn[1];
		if (ulen >= sizeof(user)) {
			ewprintf("Login name too long");
			return (NULL);
		}
		if (ulen == 0) /* ~/ or ~ */
			(void)strlcpy(user, getlogin(), sizeof(user));
		else { /* ~user/ or ~user */
			memcpy(user, &fn[1], ulen);
			user[ulen] = '\0';
		}
		pw = getpwnam(user);
		if (pw == NULL) {
			ewprintf("Unknown user %s", user);
			return (NULL);
		}
		plen = strlcpy(path, pw->pw_dir, sizeof(path));
		if (plen == 0 || path[plen - 1] != '/') {
			if (strlcat(path, "/", sizeof(path)) >= sizeof(path)) {
				ewprintf("Path too long");
				return (NULL);
			}
		}
		fn = cp;
		if (*fn == '/')
			fn++;
	}
	if (strlcat(path, fn, sizeof(path)) >= sizeof(path)) {
		ewprintf("Path too long");
		return (NULL);
	}

	if (realpath(path, fnb) == NULL)
		(void)strlcpy(fnb, path, sizeof(fnb));

	return (fnb);
}

#ifndef NO_STARTUP
/*
 * Find a startup file for the user and return its name. As a service
 * to other pieces of code that may want to find a startup file (like
 * the terminal driver in particular), accepts a suffix to be appended
 * to the startup file name.
 */
char *
startupfile(char *suffix)
{
	static char	 file[NFILEN];
	char		*home;
	int		 ret;

	if ((home = getenv("HOME")) == NULL || *home == '\0')
		goto nohome;

	if (suffix == NULL) {
		ret = snprintf(file, sizeof(file), "%s/.mg", home);
		if (ret < 0 || ret >= sizeof(file))
			return (NULL);
	} else {
		ret = snprintf(file, sizeof(file), "%s/.mg-%s", home, suffix);
		if (ret < 0 || ret >= sizeof(file))
			return (NULL);
	}

	if (access(file, R_OK) == 0)
		return (file);
nohome:
#ifdef STARTUPFILE
	if (suffix == NULL) {
		ret = snprintf(file, sizeof(file), "%s", STARTUPFILE);
		if (ret < 0 || ret >= sizeof(file))
			return (NULL);
	} else {
		ret = snprintf(file, sizeof(file), "%s%s", STARTUPFILE,
		    suffix);
		if (ret < 0 || ret >= sizeof(file))
			return (NULL);
	}

	if (access(file, R_OK) == 0)
		return (file);
#endif /* STARTUPFILE */
	return (NULL);
}
#endif /* !NO_STARTUP */

int
copy(char *frname, char *toname)
{
	int	ifd, ofd;
	char	buf[BUFSIZ];
	mode_t	fmode = DEFFILEMODE;	/* XXX?? */
	struct	stat orig;
	ssize_t	sr;

	if ((ifd = open(frname, O_RDONLY)) == -1)
		return (FALSE);
	if (fstat(ifd, &orig) == -1) {
		ewprintf("fstat: %s", strerror(errno));
		close(ifd);
		return (FALSE);
	}

	if ((ofd = open(toname, O_WRONLY|O_CREAT|O_TRUNC, fmode)) == -1) {
		close(ifd);
		return (FALSE);
	}
	while ((sr = read(ifd, buf, sizeof(buf))) > 0) {
		if (write(ofd, buf, (size_t)sr) != sr) {
			ewprintf("write error : %s", strerror(errno));
			break;
		}
	}
	if (fchmod(ofd, orig.st_mode) == -1)
		ewprintf("Cannot set original mode : %s", strerror(errno));

	if (sr == -1) {
		ewprintf("Read error : %s", strerror(errno));
		close(ifd);
		close(ofd);
		return (FALSE);
	}
	/*
	 * It is "normal" for this to fail since we can't guarantee that
	 * we will be running as root.
	 */
	if (fchown(ofd, orig.st_uid, orig.st_gid) && errno != EPERM)
		ewprintf("Cannot set owner : %s", strerror(errno));

	(void) close(ifd);
	(void) close(ofd);

	return (TRUE);
}

/*
 * return list of file names that match the name in buf.
 */
struct list *
make_file_list(char *buf)
{
	char		*dir, *file, *cp;
	size_t		 len, preflen;
	int		 ret;
	DIR		*dirp;
	struct dirent	*dent;
	struct list	*last, *current;
	char		 fl_name[NFILEN + 2];
	char		 prefixx[NFILEN + 1];

	/*
	 * We need three different strings:

	 * dir - the name of the directory containing what the user typed.
	 *  Must be a real unix file name, e.g. no ~user, etc..
	 *  Must not end in /.
	 * prefix - the portion of what the user typed that is before the
	 *  names we are going to find in the directory.  Must have a
	 * trailing / if the user typed it.
	 * names from the directory - We open dir, and return prefix
	 * concatenated with names.
	 */

	/* first we get a directory name we can look up */
	/*
	 * Names ending in . are potentially odd, because adjustname will
	 * treat foo/bar/.. as a foo/, whereas we are
	 * interested in names starting with ..
	 */
	len = strlen(buf);
	if (len && buf[len - 1] == '.') {
		buf[len - 1] = 'x';
		dir = adjustname(buf, TRUE);
		buf[len - 1] = '.';
	} else
		dir = adjustname(buf, TRUE);
	if (dir == NULL)
		return (NULL);
	/*
	 * If the user typed a trailing / or the empty string
	 * he wants us to use his file spec as a directory name.
	 */
	if (len && buf[len - 1] != '/') {
		file = strrchr(dir, '/');
		if (file) {
			*file = '\0';
			if (*dir == '\0')
				dir = "/";
		} else
			return (NULL);
	}
	/* Now we get the prefix of the name the user typed. */
	if (strlcpy(prefixx, buf, sizeof(prefixx)) >= sizeof(prefixx))
		return (NULL);
	cp = strrchr(prefixx, '/');
	if (cp == NULL)
		prefixx[0] = '\0';
	else
		cp[1] = '\0';

	preflen = strlen(prefixx);
	/* cp is the tail of buf that really needs to be compared. */
	cp = buf + preflen;
	len = strlen(cp);

	/*
	 * Now make sure that file names will fit in the buffers allocated.
	 * SV files are fairly short.  For BSD, something more general would
	 * be required.
	 */
	if (preflen > NFILEN - MAXNAMLEN)
		return (NULL);

	/* loop over the specified directory, making up the list of files */

	/*
	 * Note that it is worth our time to filter out names that don't
	 * match, even though our caller is going to do so again, and to
	 * avoid doing the stat if completion is being done, because stat'ing
	 * every file in the directory is relatively expensive.
	 */

	dirp = opendir(dir);
	if (dirp == NULL)
		return (NULL);
	last = NULL;

	while ((dent = readdir(dirp)) != NULL) {
		int isdir;

		if (dent->d_namlen < len || memcmp(cp, dent->d_name, len) != 0)
			continue;

		isdir = 0;
		if (dent->d_type == DT_DIR) {
			isdir = 1;
		} else if (dent->d_type == DT_LNK ||
			    dent->d_type == DT_UNKNOWN) {
			struct stat	statbuf;
			char		statname[NFILEN + 2];

			statbuf.st_mode = 0;
			ret = snprintf(statname, sizeof(statname), "%s/%s",
			    dir, dent->d_name);
			if (ret < 0 || ret > sizeof(statname) - 1)
				continue;
			if (stat(statname, &statbuf) < 0)
				continue;
			if (S_ISDIR(statbuf.st_mode))
				isdir = 1;
		}

		if ((current = malloc(sizeof(struct list))) == NULL) {
			free_file_list(last);
			return (NULL);
		}
		ret = snprintf(fl_name, sizeof(fl_name),
		    "%s%s%s", prefixx, dent->d_name, isdir ? "/" : "");
		if (ret < 0 || ret >= sizeof(fl_name)) {
			free(current);
			continue;
		}
		current->l_next = last;
		current->l_name = strdup(fl_name);
		last = current;
	}
	closedir(dirp);

	return (last);
}

/*
 * Test if a supplied filename refers to a directory
 * Returns ABORT on error, TRUE if directory. FALSE otherwise
 */
int
fisdir(const char *fname)
{
	struct stat	statbuf;

	if (stat(fname, &statbuf) != 0)
		return (ABORT);

	if (S_ISDIR(statbuf.st_mode))
		return (TRUE);

	return (FALSE);
}
