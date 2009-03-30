/*	$FabBSD$	*/
/*	$OpenBSD: kdump.c,v 1.39 2007/05/29 02:01:03 deraadt Exp $	*/

/*-
 * Copyright (c) 1988, 1993
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
static const char copyright[] =
"@(#) Copyright (c) 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)kdump.c	8.4 (Berkeley) 4/28/95";
#endif
static const char rcsid[] = "$OpenBSD: kdump.c,v 1.39 2007/05/29 02:01:03 deraadt Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/ktrace.h>
#include <sys/ioctl.h>
#include <sys/ptrace.h>
#include <sys/sysctl.h>
#define _KERNEL
#include <sys/errno.h>
#undef _KERNEL

#include <ctype.h>
#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vis.h>

#include "ktrace.h"
#include "kdump.h"
#include "extern.h"

int timestamp, decimal, iohex, fancy = 1, tail, maxdata;
char *tracefile = DEF_TRACEFILE;
struct ktr_header ktr_header;
pid_t pid = -1;

#define eqs(s1, s2)	(strcmp((s1), (s2)) == 0)

#include <sys/syscall.h>

#define KTRACE
#define PTRACE
#define NFSCLIENT
#define NFSSERVER
#define SYSVSEM
#define SYSVMSG
#define SYSVSHM
#define LFS
#define RTHREADS
#include <kern/syscalls.c>
#undef KTRACE
#undef PTRACE
#undef NFSCLIENT
#undef NFSSERVER
#undef SYSVSEM
#undef SYSVMSG
#undef SYSVSHM
#undef LFS
#undef RTHREADS

struct emulation {
	char *name;		/* Emulation name */
	char **sysnames;	/* Array of system call names */
	int  nsysnames;		/* Number of */
};

static struct emulation emulations[] = {
	{ "native",	syscallnames,		SYS_MAXSYSCALL },
	{ NULL,		NULL,			NULL }
};

struct emulation *current;


static char *ptrace_ops[] = {
	"PT_TRACE_ME",	"PT_READ_I",	"PT_READ_D",	"PT_READ_U",
	"PT_WRITE_I",	"PT_WRITE_D",	"PT_WRITE_U",	"PT_CONTINUE",
	"PT_KILL",	"PT_ATTACH",	"PT_DETACH",	"PT_IO",
};

static int fread_tail(void *, size_t, size_t);
static void dumpheader(struct ktr_header *);
static void ktrcsw(struct ktr_csw *);
static void ktremul(char *, size_t);
static void ktrgenio(struct ktr_genio *, size_t);
static void ktrnamei(const char *, size_t);
static void ktrpsig(struct ktr_psig *);
static void ktrsyscall(struct ktr_syscall *);
static void ktrsysret(struct ktr_sysret *);
static void setemul(const char *);
static void usage(void);

int
main(int argc, char *argv[])
{
	int ch, silent;
	size_t ktrlen, size;
	int trpoints = ALL_POINTS;
	void *m;

	current = &emulations[0];	/* native */

	while ((ch = getopt(argc, argv, "e:f:dlm:nRp:Tt:xX")) != -1)
		switch (ch) {
		case 'e':
			setemul(optarg);
			break;
		case 'f':
			tracefile = optarg;
			break;
		case 'd':
			decimal = 1;
			break;
		case 'l':
			tail = 1;
			break;
		case 'm':
			maxdata = atoi(optarg);
			break;
		case 'n':
			fancy = 0;
			break;
		case 'p':
			pid = atoi(optarg);
			break;
		case 'R':
			timestamp = 2;	/* relative timestamp */
			break;
		case 'T':
			timestamp = 1;
			break;
		case 't':
			trpoints = getpoints(optarg);
			if (trpoints < 0)
				errx(1, "unknown trace point in %s", optarg);
			break;
		case 'x':
			iohex = 1;
			break;
		case 'X':
			iohex = 2;
			break;
		default:
			usage();
		}
	if (argc > optind)
		usage();

	m = malloc(size = 1025);
	if (m == NULL)
		err(1, NULL);
	if (!freopen(tracefile, "r", stdin))
		err(1, "%s", tracefile);
	while (fread_tail(&ktr_header, sizeof(struct ktr_header), 1)) {
		silent = 0;
		if (pid != -1 && pid != ktr_header.ktr_pid)
			silent = 1;
		if (silent == 0 && trpoints & (1<<ktr_header.ktr_type))
			dumpheader(&ktr_header);
		ktrlen = ktr_header.ktr_len;
		if (ktrlen > size) {
			void *newm;

			newm = realloc(m, ktrlen+1);
			if (newm == NULL)
				err(1, NULL);
			m = newm;
			size = ktrlen;
		}
		if (ktrlen && fread_tail(m, ktrlen, 1) == 0)
			errx(1, "data too short");
		if (silent)
			continue;
		if ((trpoints & (1<<ktr_header.ktr_type)) == 0)
			continue;
		switch (ktr_header.ktr_type) {
		case KTR_SYSCALL:
			ktrsyscall((struct ktr_syscall *)m);
			break;
		case KTR_SYSRET:
			ktrsysret((struct ktr_sysret *)m);
			break;
		case KTR_NAMEI:
			ktrnamei(m, ktrlen);
			break;
		case KTR_GENIO:
			ktrgenio((struct ktr_genio *)m, ktrlen);
			break;
		case KTR_PSIG:
			ktrpsig((struct ktr_psig *)m);
			break;
		case KTR_CSW:
			ktrcsw((struct ktr_csw *)m);
			break;
		case KTR_EMUL:
			ktremul(m, ktrlen);
			break;
		}
		if (tail)
			(void)fflush(stdout);
	}
	exit(0);
}

static int
fread_tail(void *buf, size_t size, size_t num)
{
	int i;

	while ((i = fread(buf, size, num, stdin)) == 0 && tail) {
		(void)sleep(1);
		clearerr(stdin);
	}
	return (i);
}

static void
dumpheader(struct ktr_header *kth)
{
	static struct timeval prevtime;
	char unknown[64], *type;
	struct timeval temp;

	switch (kth->ktr_type) {
	case KTR_SYSCALL:
		type = "CALL";
		break;
	case KTR_SYSRET:
		type = "RET ";
		break;
	case KTR_NAMEI:
		type = "NAMI";
		break;
	case KTR_GENIO:
		type = "GIO ";
		break;
	case KTR_PSIG:
		type = "PSIG";
		break;
	case KTR_CSW:
		type = "CSW";
		break;
	case KTR_EMUL:
		type = "EMUL";
		break;
	default:
		(void)snprintf(unknown, sizeof unknown, "UNKNOWN(%d)",
		    kth->ktr_type);
		type = unknown;
	}

	(void)printf("%6ld %-8.*s ", (long)kth->ktr_pid, MAXCOMLEN,
	    kth->ktr_comm);
	if (timestamp) {
		if (timestamp == 2) {
			timersub(&kth->ktr_time, &prevtime, &temp);
			prevtime = kth->ktr_time;
		} else
			temp = kth->ktr_time;
		(void)printf("%ld.%06ld ", temp.tv_sec, temp.tv_usec);
	}
	(void)printf("%s  ", type);
}

static void
ioctldecode(u_long cmd)
{
	char dirbuf[4], *dir = dirbuf;

	if (cmd & IOC_IN)
		*dir++ = 'W';
	if (cmd & IOC_OUT)
		*dir++ = 'R';
	*dir = '\0';

	printf(decimal ? ",_IO%s('%c',%lu" : ",_IO%s('%c',%#lx",
	    dirbuf, (int)((cmd >> 8) & 0xff), cmd & 0xff);
	if ((cmd & IOC_VOID) == 0)
		printf(decimal ? ",%lu)" : ",%#lx)", (cmd >> 16) & 0xff);
	else
		printf(")");
}

static void
ktrsyscall(struct ktr_syscall *ktr)
{
	int argsize = ktr->ktr_argsize;
	register_t *ap;

	if (ktr->ktr_code >= current->nsysnames || ktr->ktr_code < 0)
		(void)printf("[%d]", ktr->ktr_code);
	else
		(void)printf("%s", current->sysnames[ktr->ktr_code]);
	ap = (register_t *)((char *)ktr + sizeof(struct ktr_syscall));
	(void)putchar('(');
	if (argsize) {
		char c = '\0';
		if (fancy) {
			if (ktr->ktr_code == SYS_ioctl) {
				const char *cp;

				if (decimal)
					(void)printf("%ld", (long)*ap);
				else
					(void)printf("%#lx", (long)*ap);
				ap++;
				argsize -= sizeof(register_t);
				if ((cp = ioctlname(*ap)) != NULL)
					(void)printf(",%s", cp);
				else
					ioctldecode(*ap);
				c = ',';
				ap++;
				argsize -= sizeof(register_t);
			} else if (ktr->ktr_code == SYS___sysctl) {
				int *np, n;

				n = ap[1];
				if (n > CTL_MAXNAME)
					n = CTL_MAXNAME;
				np = (int *)(ap + 6);
				for (; n--; np++) {
					if (c)
						putchar(c);
					printf("%d", *np);
					c = '.';
				}

				c = ',';
				ap += 2;
				argsize -= 2 * sizeof(register_t);
			} else if (ktr->ktr_code == SYS_ptrace) {
				if (*ap >= 0 && *ap <
				    sizeof(ptrace_ops) / sizeof(ptrace_ops[0]))
					(void)printf("%s", ptrace_ops[*ap]);
				else switch(*ap) {
#ifdef PT_GETFPREGS
				case PT_GETFPREGS:
					(void)printf("PT_GETFPREGS");
					break;
#endif
				case PT_GETREGS:
					(void)printf("PT_GETREGS");
					break;
#ifdef PT_SETFPREGS
				case PT_SETFPREGS:
					(void)printf("PT_SETFPREGS");
					break;
#endif
				case PT_SETREGS:
					(void)printf("PT_SETREGS");
					break;
#ifdef PT_STEP
				case PT_STEP:
					(void)printf("PT_STEP");
					break;
#endif
#ifdef PT_WCOOKIE
				case PT_WCOOKIE:
					(void)printf("PT_WCOOKIE");
					break;
#endif
				default:
					(void)printf("%ld", (long)*ap);
					break;
				}
				c = ',';
				ap++;
				argsize -= sizeof(register_t);
			}
		}
		while (argsize) {
			if (c)
				putchar(c);
			if (decimal)
				(void)printf("%ld", (long)*ap);
			else
				(void)printf("%#lx", (long)*ap);
			c = ',';
			ap++;
			argsize -= sizeof(register_t);
		}
	}
	(void)printf(")\n");
}

static void
ktrsysret(struct ktr_sysret *ktr)
{
	int ret = ktr->ktr_retval;
	int error = ktr->ktr_error;
	int code = ktr->ktr_code;

	if (code >= current->nsysnames || code < 0)
		(void)printf("[%d] ", code);
	else
		(void)printf("%s ", current->sysnames[code]);

	if (error == 0) {
		if (fancy) {
			(void)printf("%d", ret);
			if (ret < 0 || ret > 9)
				(void)printf("/%#x", ret);
		} else {
			if (decimal)
				(void)printf("%d", ret);
			else
				(void)printf("%#x", ret);
		}
	} else if (error == ERESTART)
		(void)printf("RESTART");
	else if (error == EJUSTRETURN)
		(void)printf("JUSTRETURN");
	else {
		(void)printf("-1 errno %d", ktr->ktr_error);
		if (fancy)
			(void)printf(" %s", strerror(ktr->ktr_error));
	}
	(void)putchar('\n');
}

static void
ktrnamei(const char *cp, size_t len)
{
	(void)printf("\"%.*s\"\n", (int)len, cp);
}

static void
ktremul(char *cp, size_t len)
{
	char name[1024];

	if (len >= sizeof(name))
		errx(1, "Emulation name too long");

	strncpy(name, cp, len);
	name[len] = '\0';
	(void)printf("\"%s\"\n", name);

	setemul(name);
}

static void
ktrgenio(struct ktr_genio *ktr, size_t len)
{
	unsigned char *dp = (unsigned char *)ktr + sizeof(struct ktr_genio);
	int i, j;
	size_t datalen = len - sizeof(struct ktr_genio);
	static int screenwidth = 0;
	int col = 0, width, bpl;
	unsigned char visbuf[5], *cp, c;

	if (screenwidth == 0) {
		struct winsize ws;

		if (fancy && ioctl(fileno(stderr), TIOCGWINSZ, &ws) != -1 &&
		    ws.ws_col > 8)
			screenwidth = ws.ws_col;
		else
			screenwidth = 80;
	}
	printf("fd %d %s %zu bytes\n", ktr->ktr_fd,
		ktr->ktr_rw == UIO_READ ? "read" : "wrote", datalen);
	if (maxdata && datalen > maxdata)
		datalen = maxdata;
	if (iohex && !datalen)
		return;
	if (iohex == 1) {
		putchar('\t');
		col = 8;
		for (i = 0; i < datalen; i++) {
			printf("%02x", dp[i]);
			col += 3;
			if (i < datalen - 1) {
				if (col + 3 > screenwidth) {
					printf("\n\t");
					col = 8;
				} else
					putchar(' ');
			}
		}
		putchar('\n');
		return;
	}
	if (iohex == 2) {
		bpl = (screenwidth - 13)/4;
		if (bpl <= 0)
			bpl = 1;
		for (i = 0; i < datalen; i += bpl) {
			printf("   %04x:  ", i);
			for (j = 0; j < bpl; j++) {
				if (i+j >= datalen)
					printf("   ");
				else
					printf("%02x ", dp[i+j]);
			}
			putchar(' ');
			for (j = 0; j < bpl; j++) {
				if (i+j >= datalen)
					break;
				c = dp[i+j];
				if (!isprint(c))
					c = '.';
				putchar(c);
			}
			putchar('\n');
		}
		return;
	}
	(void)printf("       \"");
	col = 8;
	for (; datalen > 0; datalen--, dp++) {
		(void)vis(visbuf, *dp, VIS_CSTYLE, *(dp+1));
		cp = visbuf;

		/*
		 * Keep track of printables and
		 * space chars (like fold(1)).
		 */
		if (col == 0) {
			(void)putchar('\t');
			col = 8;
		}
		switch (*cp) {
		case '\n':
			col = 0;
			(void)putchar('\n');
			continue;
		case '\t':
			width = 8 - (col&07);
			break;
		default:
			width = strlen(cp);
		}
		if (col + width > (screenwidth-2)) {
			(void)printf("\\\n\t");
			col = 8;
		}
		col += width;
		do {
			(void)putchar(*cp++);
		} while (*cp);
	}
	if (col == 0)
		(void)printf("       ");
	(void)printf("\"\n");
}

static void
ktrpsig(struct ktr_psig *psig)
{
	(void)printf("SIG%s ", sys_signame[psig->signo]);
	if (psig->action == SIG_DFL)
		(void)printf("SIG_DFL code %d", psig->code);
	else
		(void)printf("caught handler=0x%lx mask=0x%x",
		    (u_long)psig->action, psig->mask);
	switch (psig->signo) {
	case SIGSEGV:
	case SIGILL:
	case SIGBUS:
	case SIGFPE:
		printf(" addr=%p trapno=%d", psig->si.si_addr,
		    psig->si.si_trapno);
		break;
	default:
		break;
	}
	printf("\n");
}

static void
ktrcsw(struct ktr_csw *cs)
{
	(void)printf("%s %s\n", cs->out ? "stop" : "resume",
	    cs->user ? "user" : "kernel");
}

static void
usage(void)
{

	extern char *__progname;
	fprintf(stderr, "usage: %s "
	    "[-dlnRTXx] [-e emulation] [-p pid] [-f trfile] [-m maxdata] "
	    "[-t [ceinsw]]\n", __progname);
	exit(1);
}

static void
setemul(const char *name)
{
	int i;

	for (i = 0; emulations[i].name != NULL; i++)
		if (strcmp(emulations[i].name, name) == 0) {
			current = &emulations[i];
			return;
		}
	warnx("Emulation `%s' unknown", name);
}
