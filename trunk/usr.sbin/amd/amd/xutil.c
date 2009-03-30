/*
 * Copyright (c) 1990 Jan-Simon Pendry
 * Copyright (c) 1990 Imperial College of Science, Technology & Medicine
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry at Imperial College, London.
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
 *
 *	from: @(#)xutil.c	8.1 (Berkeley) 6/6/93
 *	$Id: xutil.c,v 1.11 2003/06/02 23:36:51 millert Exp $
 */

#include "config.h"
#ifdef HAS_SYSLOG
#include <syslog.h>
#endif /* HAS_SYSLOG */
#ifdef HAS_STRERROR
#include <string.h>
#endif

#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>

FILE *logfp = stderr;		/* Log errors to stderr initially */
#ifdef HAS_SYSLOG
int syslogging;
#endif /* HAS_SYSLOG */
int xlog_level = XLOG_ALL & ~XLOG_MAP & ~XLOG_STATS;
int xlog_level_init = ~0;

/*
 * List of log options
 */
struct opt_tab xlog_opt[] = {
	{ "all", XLOG_ALL },		/* All messages */
#ifdef DEBUG
	{ "debug", XLOG_DEBUG },	/* Debug messages */
#endif /* DEBUG */
	{ "error", XLOG_ERROR },	/* Non-fatal system errors */
	{ "fatal", XLOG_FATAL },	/* Fatal errors */
	{ "info", XLOG_INFO },		/* Information */
	{ "map", XLOG_MAP },		/* Map errors */
	{ "stats", XLOG_STATS },	/* Additional statistical information */
	{ "user", XLOG_USER },		/* Non-fatal user errors */
	{ "warn", XLOG_WARNING },	/* Warnings */
	{ "warning", XLOG_WARNING },	/* Warnings */
	{ 0, 0 }
};

void *
xmalloc(int len)
{
	void *p;
	int retries = 600;

	/*
	 * Avoid malloc's which return NULL for malloc(0)
	 */
	if (len == 0)
		len = 1;

	do {
		p = (void *)malloc((unsigned) len);
		if (p) {
#if defined(DEBUG) && defined(DEBUG_MEM)
			Debug(D_MEM) plog(XLOG_DEBUG, "Allocated size %d; block %#x", len, p);
#endif /* defined(DEBUG) && defined(DEBUG_MEM) */
			return p;
		}
		if (retries > 0) {
			plog(XLOG_ERROR, "Retrying memory allocation");
			sleep(1);
		}
	} while (--retries);

	plog(XLOG_FATAL, "Out of memory");
	going_down(1);

	abort();

	return 0;
}

void *
xrealloc(void *ptr, int len)
{
#if defined(DEBUG) && defined(DEBUG_MEM)
	Debug(D_MEM) plog(XLOG_DEBUG, "Reallocated size %d; block %#x", len, ptr);
#endif /* defined(DEBUG) && defined(DEBUG_MEM) */

	if (len == 0)
		len = 1;

	if (ptr)
		ptr = (void *)realloc(ptr, (unsigned) len);
	else
		ptr = (void *)xmalloc((unsigned) len);

	if (!ptr) {
		plog(XLOG_FATAL, "Out of memory in realloc");
		going_down(1);
		abort();
	}
	return ptr;
}

#if defined(DEBUG) && defined(DEBUG_MEM)
xfree(char *f, int l, void *p)
{
	Debug(D_MEM) plog(XLOG_DEBUG, "Free in %s:%d: block %#x", f, l, p);
#undef free
	free(p);
}
#endif /* defined(DEBUG) && defined(DEBUG_MEM) */
#ifdef DEBUG_MEM
static int mem_bytes;
static int orig_mem_bytes;

static void
checkup_mem(void)
{
	extern struct mallinfo __mallinfo;
	if (mem_bytes != __mallinfo.uordbytes) {
		if (orig_mem_bytes == 0)
			mem_bytes = orig_mem_bytes = __mallinfo.uordbytes;
		else {
			fprintf(logfp, "%s[%ld]: ", __progname, (long)mypid);
			if (mem_bytes < __mallinfo.uordbytes) {
				fprintf(logfp, "ALLOC: %d bytes",
					__mallinfo.uordbytes - mem_bytes);
			} else {
				fprintf(logfp, "FREE: %d bytes",
					mem_bytes - __mallinfo.uordbytes);
			}
			mem_bytes = __mallinfo.uordbytes;
			fprintf(logfp, ", making %d missing\n",
				mem_bytes - orig_mem_bytes);
		}
	}
	malloc_verify();
}
#endif /* DEBUG_MEM */

/*
 * Take a log format string and expand occurrences of %m
 * with the current error code taken from errno.  Make sure
 * 'e' never gets longer than maxlen characters.
 */
INLINE static void
expand_error(char *f, char *e, int maxlen)
{
#ifndef HAS_STRERROR
	extern int sys_nerr;
	extern char *sys_errlist[];
#endif
	char *p, *q;
	int error = errno;
	int len = 0;

	for (p = f, q = e; (*q = *p) && len < maxlen; len++, q++, p++) {
		if (p[0] == '%' && p[1] == 'm') {
			char *errstr;
#ifdef HAS_STRERROR
			errstr = strerror(error);
#else
			if (error < 0 || error >= sys_nerr)
				errstr = 0;
			else
				errstr = sys_errlist[error];
#endif
			if (errstr)
				strlcpy(q, errstr, maxlen - (q - e));
			else
				snprintf(q, maxlen - (q - e),
				    "Error %d", error);
			len += strlen(q) - 1;
			q += strlen(q) - 1;
			p++;
		}
	}
	e[maxlen-1] = '\0';		/* null terminate, to be sure */
}

/*
 * Output the time of day and hostname to the logfile
 */
static void
show_time_host_and_name(int lvl)
{
	static time_t last_t = 0;
	static char *last_ctime = 0;
	time_t t = clocktime();
	char *sev;
	extern char *ctime();

#if defined(DEBUG) && defined(PARANOID)
extern char **gargv;
#endif /* defined(DEBUG) && defined(PARANOID) */

	if (t != last_t) {
		last_ctime = ctime(&t);
		last_t = t;
	}

	switch (lvl) {
	case XLOG_FATAL:	sev = "fatal:"; break;
	case XLOG_ERROR:	sev = "error:"; break;
	case XLOG_USER:		sev = "user: "; break;
	case XLOG_WARNING:	sev = "warn: "; break;
	case XLOG_INFO:		sev = "info: "; break;
	case XLOG_DEBUG:	sev = "debug:"; break;
	case XLOG_MAP:		sev = "map:  "; break;
	case XLOG_STATS:	sev = "stats:"; break;
	default:		sev = "hmm:  "; break;
	}
	fprintf(logfp, "%15.15s %s %s[%ld]/%s ",
		last_ctime+4, hostname,
#if defined(DEBUG) && defined(PARANOID)
		gargv[0],
#else
		__progname,
#endif /* defined(DEBUG) && defined(PARANOID) */
		(long)mypid,
		sev);
}

/*VARARGS1*/
void
plog(int lvl, char *fmt, ...)
{
	char msg[1024];
	char efmt[1024];
	char *ptr;
	va_list ap;

	va_start(ap, fmt);

	if (!(xlog_level & lvl))
		return;

#ifdef DEBUG_MEM
	checkup_mem();
#endif /* DEBUG_MEM */

	expand_error(fmt, efmt, sizeof(efmt));
	/*
	 * XXX: msg is 1024 bytes long.  It is possible to write into it
	 * more than 1024 bytes, if efmt is already large, and vargs expand
	 * as well.
	 */
	vsnprintf(msg, sizeof(msg), efmt, ap);
	ptr = msg + strlen(msg);
	if (ptr[-1] == '\n')
		*--ptr  = '\0';
#ifdef HAS_SYSLOG
	if (syslogging) {
		switch(lvl) {	/* from mike <mcooper@usc.edu> */
		case XLOG_FATAL:	lvl = LOG_CRIT; break;
		case XLOG_ERROR:	lvl = LOG_ERR; break;
		case XLOG_USER:		lvl = LOG_WARNING; break;
		case XLOG_WARNING:	lvl = LOG_WARNING; break;
		case XLOG_INFO:		lvl = LOG_INFO; break;
		case XLOG_DEBUG:	lvl = LOG_DEBUG; break;
		case XLOG_MAP:		lvl = LOG_DEBUG; break;
		case XLOG_STATS:	lvl = LOG_INFO; break;
		default:		lvl = LOG_ERR; break;
		}
		syslog(lvl, "%s", msg);
		return;
	}
#endif /* HAS_SYSLOG */

	/*
	 * Mimic syslog header
	 */
	va_end(ap);
	show_time_host_and_name(lvl);
	fwrite(msg, ptr - msg, 1, logfp);
	fputc('\n', logfp);
	fflush(logfp);
}

void
show_opts(int ch, struct opt_tab *opts)
{
	/*
	 * Display current debug options
	 */
	int i;
	int s = '{';

	fprintf(stderr, "\t[-%c {no}", ch);
	for (i = 0; opts[i].opt; i++) {
		fprintf(stderr, "%c%s", s, opts[i].opt);
		s = ',';
	}
	fputs("}]\n", stderr);
}

int
cmdoption(char *s, struct opt_tab *optb, int *flags)
{
	char *p = s;
	int errs = 0;

	while (p && *p) {
		int neg;
		char *opt;
		struct opt_tab *dp, *dpn = 0;

		s = p;
		p = strchr(p, ',');
		if (p)
			*p = '\0';

		if (s[0] == 'n' && s[1] == 'o') {
			opt = s + 2;
			neg = 1;
		} else {
			opt = s;
			neg = 0;
		}

		/*
		 * Scan the array of debug options to find the
		 * corresponding flag value.  If it is found
		 * then set (or clear) the flag (depending on
		 * whether the option was prefixed with "no").
		 */
		for (dp = optb; dp->opt; dp++) {
			if (strcmp(opt, dp->opt) == 0)
				break;
			if (opt != s && !dpn && strcmp(s, dp->opt) == 0)
				dpn = dp;
		}

		if (dp->opt || dpn) {
			if (!dp->opt) {
				dp = dpn;
				neg = !neg;
			}
			if (neg)
				*flags &= ~dp->flag;
			else
				*flags |= dp->flag;
		} else {
			/*
			 * This will log to stderr when parsing the command line
			 * since any -l option will not yet have taken effect.
			 */
			plog(XLOG_USER, "option \"%s\" not recognised", s);
			errs++;
		}
		/*
		 * Put the comma back
		 */
		if (p)
			*p++ = ',';
	}

	return errs;
}

/*
 * Switch on/off logging options
 */
int
switch_option(char *opt)
{
	int xl = xlog_level;
	int rc = cmdoption(opt, xlog_opt, &xl);
	if (rc) {
		rc = EINVAL;
	} else {
		/*
		 * Keep track of initial log level, and
		 * don't allow options to be turned off.
		 */
		if (xlog_level_init == ~0)
			xlog_level_init = xl;
		else
			xl |= xlog_level_init;
		xlog_level = xl;
	}
	return rc;
}

/*
 * Change current logfile
 */
int
switch_to_logfile(char *logfile)
{
	FILE *new_logfp = stderr;

	if (logfile) {
#ifdef HAS_SYSLOG
		syslogging = 0;
#endif /* HAS_SYSLOG */
		if (strcmp(logfile, "/dev/stderr") == 0)
			new_logfp = stderr;
		else if (strcmp(logfile, "syslog") == 0) {
#ifdef HAS_SYSLOG
			syslogging = 1;
			new_logfp = stderr;
#if defined(LOG_CONS) && defined(LOG_NOWAIT)
			openlog(__progname, LOG_PID|LOG_CONS|LOG_NOWAIT,
				LOG_DAEMON);
#else
			/* 4.2 compat mode - XXX */
			openlog(__progname, LOG_PID);
#endif /* LOG_CONS && LOG_NOWAIT */
#else
			plog(XLOG_WARNING, "syslog option not supported, logging unchanged");
#endif /* HAS_SYSLOG */
		} else {
			(void) umask(orig_umask);
			new_logfp = fopen(logfile, "a");
			umask(0);
		}
	}

	/*
	 * If we couldn't open a new file, then continue using the old.
	 */
	if (!new_logfp && logfile) {
		plog(XLOG_USER, "%s: Can't open logfile: %m", logfile);
		return 1;
	}
	/*
	 * Close the previous file
	 */
	if (logfp && logfp != stderr)
		(void) fclose(logfp);
	logfp = new_logfp;
	return 0;
}

time_t clock_valid = 0;
time_t xclock_valid = 0;
#ifndef clocktime
time_t
clocktime(void)
{
	time_t now = time(&clock_valid);
	if (xclock_valid > now) {
		/*
		 * Someone set the clock back!
		 */
		plog(XLOG_WARNING, "system clock reset");
		reschedule_timeouts(now, xclock_valid);
	}
	return xclock_valid = now;
}
#endif /* clocktime */
