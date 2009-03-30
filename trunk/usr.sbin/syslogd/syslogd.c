/*	$OpenBSD: syslogd.c,v 1.101 2008/04/21 22:09:51 mpf Exp $	*/

/*
 * Copyright (c) 1983, 1988, 1993, 1994
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
"@(#) Copyright (c) 1983, 1988, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static const char sccsid[] = "@(#)syslogd.c	8.3 (Berkeley) 4/4/94";
#else
static const char rcsid[] = "$OpenBSD: syslogd.c,v 1.101 2008/04/21 22:09:51 mpf Exp $";
#endif
#endif /* not lint */

/*
 *  syslogd -- log system messages
 *
 * This program implements a system log. It takes a series of lines.
 * Each line may have a priority, signified as "<n>" as
 * the first characters of the line.  If this is
 * not present, a default priority is used.
 *
 * To kill syslogd, send a signal 15 (terminate).  A signal 1 (hup) will
 * cause it to reread its configuration file.
 *
 * Defined Constants:
 *
 * MAXLINE -- the maximum line length that can be handled.
 * DEFUPRI -- the default priority for user messages
 * DEFSPRI -- the default priority for kernel messages
 *
 * Author: Eric Allman
 * extensive changes by Ralph Campbell
 * more extensive changes by Eric Allman (again)
 * memory buffer logging by Damien Miller
 */

#define	MAXLINE		1024		/* maximum line length */
#define MIN_MEMBUF	(MAXLINE * 4)	/* Minimum memory buffer size */
#define MAX_MEMBUF	(256 * 1024)	/* Maximum memory buffer size */
#define MAX_MEMBUF_NAME	64		/* Max length of membuf log name */
#define	MAXSVLINE	120		/* maximum saved line length */
#define DEFUPRI		(LOG_USER|LOG_NOTICE)
#define DEFSPRI		(LOG_KERN|LOG_CRIT)
#define TIMERINTVL	30		/* interval for checking flush, mark */
#define TTYMSGTIME	1		/* timeout passed to ttymsg */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/msgbuf.h>
#include <sys/uio.h>
#include <sys/sysctl.h>
#include <sys/un.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <err.h>
#include <fcntl.h>
#include <paths.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utmp.h>
#include <vis.h>

#define SYSLOG_NAMES
#include <sys/syslog.h>

#include "syslogd.h"

char *ConfFile = _PATH_LOGCONF;
const char ctty[] = _PATH_CONSOLE;

#define MAXUNAMES	20	/* maximum number of user names */


/*
 * Flags to logmsg().
 */

#define IGN_CONS	0x001	/* don't print on console */
#define SYNC_FILE	0x002	/* do fsync on file after printing */
#define ADDDATE		0x004	/* add a date to the message */
#define MARK		0x008	/* this message is a mark */

/*
 * This structure represents the files that will have log
 * copies printed.
 */

struct filed {
	struct	filed *f_next;		/* next in linked list */
	short	f_type;			/* entry type, see below */
	short	f_file;			/* file descriptor */
	time_t	f_time;			/* time this was last written */
	u_char	f_pmask[LOG_NFACILITIES+1];	/* priority mask */
	char	*f_program;		/* program this applies to */
	union {
		char	f_uname[MAXUNAMES][UT_NAMESIZE+1];
		struct {
			char	f_hname[MAXHOSTNAMELEN];
			struct sockaddr_storage	f_addr;
		} f_forw;		/* forwarding address */
		char	f_fname[MAXPATHLEN];
		struct {
			char	f_mname[MAX_MEMBUF_NAME];
			struct ringbuf *f_rb;
			int	f_overflow;
			int	f_attached;
			size_t	f_len;
		} f_mb;		/* Memory buffer */
	} f_un;
	char	f_prevline[MAXSVLINE];		/* last message logged */
	char	f_lasttime[16];			/* time of last occurrence */
	char	f_prevhost[MAXHOSTNAMELEN];	/* host from which recd. */
	int	f_prevpri;			/* pri of f_prevline */
	int	f_prevlen;			/* length of f_prevline */
	int	f_prevcount;			/* repetition cnt of prevline */
	unsigned int f_repeatcount;		/* number of "repeated" msgs */
	int	f_quick;			/* abort when matched */
	time_t	f_lasterrtime;			/* last error was reported */
};

/*
 * Intervals at which we flush out "message repeated" messages,
 * in seconds after previous message is logged.  After each flush,
 * we move to the next interval until we reach the largest.
 */
int	repeatinterval[] = { 30, 120, 600 };	/* # of secs before flush */
#define	MAXREPEAT ((sizeof(repeatinterval) / sizeof(repeatinterval[0])) - 1)
#define	REPEATTIME(f)	((f)->f_time + repeatinterval[(f)->f_repeatcount])
#define	BACKOFF(f)	{ if (++(f)->f_repeatcount > MAXREPEAT) \
				(f)->f_repeatcount = MAXREPEAT; \
			}

/* values for f_type */
#define F_UNUSED	0		/* unused entry */
#define F_FILE		1		/* regular file */
#define F_TTY		2		/* terminal */
#define F_CONSOLE	3		/* console terminal */
#define F_FORW		4		/* remote machine */
#define F_USERS		5		/* list of users */
#define F_WALL		6		/* everyone logged on */
#define F_MEMBUF	7		/* memory buffer */
#define F_PIPE		8		/* pipe to external program */

char	*TypeNames[9] = {
	"UNUSED",	"FILE",		"TTY",		"CONSOLE",
	"FORW",		"USERS",	"WALL",		"MEMBUF",
	"PIPE"
};

struct	filed *Files;
struct	filed consfile;

int	nfunix = 1;		/* Number of Unix domain sockets requested */
char	*funixn[MAXFUNIX] = { _PATH_LOG }; /* Paths to Unix domain sockets */
int	Debug;			/* debug flag */
int	Startup = 1;		/* startup flag */
char	LocalHostName[MAXHOSTNAMELEN];	/* our hostname */
char	*LocalDomain;		/* our local domain name */
int	InetInuse = 0;		/* non-zero if INET sockets are being used */
int	Initialized = 0;	/* set when we have initialized ourselves */

int	MarkInterval = 20 * 60;	/* interval between marks in seconds */
int	MarkSeq = 0;		/* mark sequence number */
int	SecureMode = 1;		/* when true, speak only unix domain socks */
int	NoDNS = 0;		/* when true, will refrain from doing DNS lookups */

char	*ctlsock_path = NULL;	/* Path to control socket */

#define CTL_READING_CMD		1
#define CTL_WRITING_REPLY	2
#define CTL_WRITING_CONT_REPLY	3
int	ctl_state = 0;		/* What the control socket is up to */
int	membuf_drop = 0;	/* logs were dropped in continuous membuf read */

/*
 * Client protocol NB. all numeric fields in network byte order
 */
#define CTL_VERSION		1

/* Request */
struct	{
	u_int32_t	version;
#define CMD_READ	1	/* Read out log */
#define CMD_READ_CLEAR	2	/* Read and clear log */
#define CMD_CLEAR	3	/* Clear log */
#define CMD_LIST	4	/* List available logs */
#define CMD_FLAGS	5	/* Query flags only */
#define CMD_READ_CONT	6	/* Read out log continuously */
	u_int32_t	cmd;
	char		logname[MAX_MEMBUF_NAME];
}	ctl_cmd;

size_t	ctl_cmd_bytes = 0;	/* number of bytes of ctl_cmd read */

/* Reply */
struct ctl_reply_hdr {
	u_int32_t	version;
#define CTL_HDR_FLAG_OVERFLOW	0x01
	u_int32_t	flags;
	/* Reply text follows, up to MAX_MEMBUF long */
};

#define CTL_HDR_LEN		(sizeof(struct ctl_reply_hdr))
#define CTL_REPLY_MAXSIZE	(CTL_HDR_LEN + MAX_MEMBUF)
#define CTL_REPLY_SIZE		(strlen(reply_text) + CTL_HDR_LEN)

char	*ctl_reply = NULL;	/* Buffer for control connection reply */
char	*reply_text;		/* Start of reply text in buffer */
size_t	ctl_reply_size = 0;	/* Number of bytes used in reply */
size_t	ctl_reply_offset = 0;	/* Number of bytes of reply written so far */

struct pollfd pfd[N_PFD];

volatile sig_atomic_t MarkSet;
volatile sig_atomic_t WantDie;
volatile sig_atomic_t DoInit;

struct filed *cfline(char *, char *);
void    cvthname(struct sockaddr_in *, char *, size_t);
int	decode(const char *, const CODE *);
void	dodie(int);
void	doinit(int);
void	die(int);
void	domark(int);
void	markit(void);
void	fprintlog(struct filed *, int, char *);
void	init(void);
void	logerror(const char *);
void	logmsg(int, char *, char *, int);
struct filed *find_dup(struct filed *);
void	printline(char *, char *);
void	printsys(char *);
void	reapchild(int);
char   *ttymsg(struct iovec *, int, char *, int);
void	usage(void);
void	wallmsg(struct filed *, struct iovec *);
int	getmsgbufsize(void);
int	unix_socket(char *, int, mode_t);
void	double_rbuf(int);
void	ctlsock_accept_handler(void);
void	ctlconn_read_handler(void);
void	ctlconn_write_handler(void);
void	tailify_replytext(char *, int);
void	logto_ctlconn(char *);

int
main(int argc, char *argv[])
{
	int ch, i, linesize, fd;
	struct sockaddr_un fromunix;
	struct sockaddr_in frominet;
	socklen_t len;
	char *p, *line;
	char resolve[MAXHOSTNAMELEN];
	int lockpipe[2], nullfd;
	struct addrinfo hints, *res, *res0;
	FILE *fp;

	while ((ch = getopt(argc, argv, "dnuf:m:p:a:s:")) != -1)
		switch (ch) {
		case 'd':		/* debug */
			Debug++;
			break;
		case 'f':		/* configuration file */
			ConfFile = optarg;
			break;
		case 'm':		/* mark interval */
			MarkInterval = atoi(optarg) * 60;
			break;
		case 'n':		/* don't do DNS lookups */
			NoDNS = 1;
			break;
		case 'p':		/* path */
			funixn[0] = optarg;
			break;
		case 'u':		/* allow udp input port */
			SecureMode = 0;
			break;
		case 'a':
			if (nfunix >= MAXFUNIX)
				fprintf(stderr, "syslogd: "
				    "out of descriptors, ignoring %s\n",
				    optarg);
			else
				funixn[nfunix++] = optarg;
			break;
		case 's':
			ctlsock_path = optarg;
			break;
		default:
			usage();
		}
	if ((argc -= optind) != 0)
		usage();

	if (Debug)
		setlinebuf(stdout);

	if ((fd = nullfd = open(_PATH_DEVNULL, O_RDWR)) == -1) {
		logerror("Couldn't open /dev/null");
		die(0);
	}
	while (fd++ <= 2) {
		if (fcntl(fd, F_GETFL, 0) == -1)
			if (dup2(nullfd, fd) == -1)
				logerror("dup2");
	}

	consfile.f_type = F_CONSOLE;
	(void)strlcpy(consfile.f_un.f_fname, ctty,
	    sizeof(consfile.f_un.f_fname));
	(void)gethostname(LocalHostName, sizeof(LocalHostName));
	if ((p = strchr(LocalHostName, '.')) != NULL) {
		*p++ = '\0';
		LocalDomain = p;
	} else
		LocalDomain = "";

	linesize = getmsgbufsize();
	if (linesize < MAXLINE)
		linesize = MAXLINE;
	linesize++;
	if ((line = malloc(linesize)) == NULL) {
		logerror("Couldn't allocate line buffer");
		die(0);
	}

	/* Clear poll array, set all fds to ignore */
	for (i = 0; i < N_PFD; i++) {
		pfd[i].fd = -1;
		pfd[i].events = 0;
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
	hints.ai_flags = AI_PASSIVE;

	i = getaddrinfo(NULL, "syslog", &hints, &res0);
	if (i) {
		errno = 0;
		logerror("syslog/udp: unknown service");
		die(0);
	}

	for (res = res0; res; res = res->ai_next) {
		struct pollfd *pfdp;

		if (res->ai_family == AF_INET)
			pfdp = &pfd[PFD_INET];
		else {
			/*
			 * XXX AF_INET6 is skipped on purpose, need to
			 * fix '@' handling first.
			 */
			continue;
		}

		if (pfdp->fd >= 0)
			continue;

		fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (fd < 0)
			continue;

		if (bind(fd, res->ai_addr, res->ai_addrlen) < 0) {
			logerror("bind");
			close(fd);
			if (!Debug)
				die(0);
			fd = -1;
			continue;
		}

		InetInuse = 1;
		pfdp->fd = fd;
		if (SecureMode)
			shutdown(pfdp->fd, SHUT_RD);
		else {
			double_rbuf(pfdp->fd);
			pfdp->events = POLLIN;
		}
	}

	freeaddrinfo(res0);

#ifndef SUN_LEN
#define SUN_LEN(unp) (strlen((unp)->sun_path) + 2)
#endif
	for (i = 0; i < nfunix; i++) {
		if ((fd = unix_socket(funixn[i], SOCK_DGRAM, 0666)) == -1) {
			if (i == 0 && !Debug)
				die(0);
			continue;
		}
		double_rbuf(fd);
		pfd[PFD_UNIX_0 + i].fd = fd;
		pfd[PFD_UNIX_0 + i].events = POLLIN;
	}

	if (ctlsock_path != NULL) {
		fd = unix_socket(ctlsock_path, SOCK_STREAM, 0600);
		if (fd != -1) {
			if (listen(fd, 16) == -1) {
				logerror("ctlsock listen");
				die(0);
			}
			pfd[PFD_CTLSOCK].fd = fd;
			pfd[PFD_CTLSOCK].events = POLLIN;
		} else if (!Debug)
			die(0);
	}

	if ((fd = open(_PATH_KLOG, O_RDONLY, 0)) == -1) {
		dprintf("can't open %s (%d)\n", _PATH_KLOG, errno);
	} else {
		pfd[PFD_KLOG].fd = fd;
		pfd[PFD_KLOG].events = POLLIN;
	}

	dprintf("off & running....\n");

	chdir("/");

	tzset();

	if (!Debug) {
		char c;

		pipe(lockpipe);

		switch(fork()) {
		case -1:
			exit(1);
		case 0:
			setsid();
			close(lockpipe[0]);
			break;
		default:
			close(lockpipe[1]);
			read(lockpipe[0], &c, 1);
			_exit(0);
		}
	}

	/* tuck my process id away */
	if (!Debug) {
		fp = fopen(_PATH_LOGPID, "w");
		if (fp != NULL) {
			fprintf(fp, "%ld\n", (long)getpid());
			(void) fclose(fp);
		}
	}

	/* Privilege separation begins here */
	if (priv_init(ConfFile, NoDNS, lockpipe[1], nullfd, argv) < 0)
		errx(1, "unable to privsep");

	/* Process is now unprivileged and inside a chroot */
	init();

	Startup = 0;

	/* Allocate ctl socket reply buffer if we have a ctl socket */
	if (pfd[PFD_CTLSOCK].fd != -1 &&
	    (ctl_reply = malloc(CTL_REPLY_MAXSIZE)) == NULL) {
		logerror("Couldn't allocate ctlsock reply buffer");
		die(0);
	}
	reply_text = ctl_reply + CTL_HDR_LEN;

	if (!Debug) {
		dup2(nullfd, STDIN_FILENO);
		dup2(nullfd, STDOUT_FILENO);
		dup2(nullfd, STDERR_FILENO);
		if (nullfd > 2)
			close(nullfd);
		close(lockpipe[1]);
	}

	/*
	 * Signal to the priv process that the initial config parsing is done
	 * so that it will reject any future attempts to open more files
	 */
	priv_config_parse_done();

	(void)signal(SIGHUP, doinit);
	(void)signal(SIGTERM, dodie);
	(void)signal(SIGINT, Debug ? dodie : SIG_IGN);
	(void)signal(SIGQUIT, Debug ? dodie : SIG_IGN);
	(void)signal(SIGCHLD, reapchild);
	(void)signal(SIGALRM, domark);
	(void)signal(SIGPIPE, SIG_IGN);
	(void)alarm(TIMERINTVL);

	logmsg(LOG_SYSLOG|LOG_INFO, "syslogd: start", LocalHostName, ADDDATE);
	dprintf("syslogd: started\n");

	for (;;) {
		if (MarkSet)
			markit();
		if (WantDie)
			die(WantDie);

		if (DoInit) {
			init();
			DoInit = 0;

			logmsg(LOG_SYSLOG|LOG_INFO, "syslogd: restart",
			    LocalHostName, ADDDATE);
			dprintf("syslogd: restarted\n");
		}

		switch (poll(pfd, PFD_UNIX_0 + nfunix, -1)) {
		case 0:
			continue;
		case -1:
			if (errno != EINTR)
				logerror("poll");
			continue;
		}

		if ((pfd[PFD_KLOG].revents & POLLIN) != 0) {
			i = read(pfd[PFD_KLOG].fd, line, linesize - 1);
			if (i > 0) {
				line[i] = '\0';
				printsys(line);
			} else if (i < 0 && errno != EINTR) {
				logerror("klog");
				pfd[PFD_KLOG].fd = -1;
				pfd[PFD_KLOG].events = 0;
			}
		}
		if ((pfd[PFD_INET].revents & POLLIN) != 0) {
			len = sizeof(frominet);
			i = recvfrom(pfd[PFD_INET].fd, line, MAXLINE, 0,
			    (struct sockaddr *)&frominet, &len);
			if (i > 0) {
				line[i] = '\0';
				cvthname(&frominet, resolve,
				    sizeof resolve);
				dprintf("cvthname res: %s\n", resolve);
				printline(resolve, line);
			} else if (i < 0 && errno != EINTR)
				logerror("recvfrom inet");
		}
		if ((pfd[PFD_CTLSOCK].revents & POLLIN) != 0)
			ctlsock_accept_handler();
		if ((pfd[PFD_CTLCONN].revents & POLLIN) != 0)
			ctlconn_read_handler();
		if ((pfd[PFD_CTLCONN].revents & POLLOUT) != 0)
			ctlconn_write_handler();

		for (i = 0; i < nfunix; i++) {
			if ((pfd[PFD_UNIX_0 + i].revents & POLLIN) != 0) {
				ssize_t rlen;

				len = sizeof(fromunix);
				rlen = recvfrom(pfd[PFD_UNIX_0 + i].fd, line,
				    MAXLINE, 0, (struct sockaddr *)&fromunix,
				    &len);
				if (rlen > 0) {
					line[rlen] = '\0';
					printline(LocalHostName, line);
				} else if (rlen == -1 && errno != EINTR)
					logerror("recvfrom unix");
			}
		}
	}
	/* NOTREACHED */
	free(pfd);
	return (0);
}

void
usage(void)
{

	(void)fprintf(stderr,
	    "usage: syslogd [-dnu] [-a path] [-f config_file] [-m mark_interval]\n"
	    "               [-p log_socket] [-s reporting_socket]\n");
	exit(1);
}

/*
 * Take a raw input line, decode the message, and print the message
 * on the appropriate log files.
 */
void
printline(char *hname, char *msg)
{
	int pri;
	char *p, *q, line[MAXLINE + 1];

	/* test for special codes */
	pri = DEFUPRI;
	p = msg;
	if (*p == '<') {
		pri = 0;
		while (isdigit(*++p))
			pri = 10 * pri + (*p - '0');
		if (*p == '>')
			++p;
	}
	if (pri &~ (LOG_FACMASK|LOG_PRIMASK))
		pri = DEFUPRI;

	/*
	 * Don't allow users to log kernel messages.
	 * NOTE: since LOG_KERN == 0 this will also match
	 * messages with no facility specified.
	 */
	if (LOG_FAC(pri) == LOG_KERN)
		pri = LOG_USER | LOG_PRI(pri);

	for (q = line; *p && q < &line[sizeof(line) - 4]; p++) {
		if (*p == '\n')
			*q++ = ' ';
		else
			q = vis(q, *p, 0, 0);
	}
	*q = '\0';

	logmsg(pri, line, hname, 0);
}

/*
 * Take a raw input line from /dev/klog, split and format similar to syslog().
 */
void
printsys(char *msg)
{
	int c, pri, flags;
	char *lp, *p, *q, line[MAXLINE + 1];

	(void)snprintf(line, sizeof line, "%s: ", _PATH_UNIX);
	lp = line + strlen(line);
	for (p = msg; *p != '\0'; ) {
		flags = SYNC_FILE | ADDDATE;	/* fsync file after write */
		pri = DEFSPRI;
		if (*p == '<') {
			pri = 0;
			while (isdigit(*++p))
				pri = 10 * pri + (*p - '0');
			if (*p == '>')
				++p;
		} else {
			/* kernel printf's come out on console */
			flags |= IGN_CONS;
		}
		if (pri &~ (LOG_FACMASK|LOG_PRIMASK))
			pri = DEFSPRI;

		q = lp;
		while (*p && (c = *p++) != '\n' && q < &line[sizeof(line) - 4])
			q = vis(q, c, 0, 0);

		logmsg(pri, line, LocalHostName, flags);
	}
}

time_t	now;

/*
 * Log a message to the appropriate log files, users, etc. based on
 * the priority.
 */
void
logmsg(int pri, char *msg, char *from, int flags)
{
	struct filed *f;
	int fac, msglen, prilev, i;
	sigset_t mask, omask;
	char *timestamp;
	char prog[NAME_MAX+1];

	dprintf("logmsg: pri 0%o, flags 0x%x, from %s, msg %s\n",
	    pri, flags, from, msg);

	sigemptyset(&mask);
	sigaddset(&mask, SIGALRM);
	sigaddset(&mask, SIGHUP);
	sigprocmask(SIG_BLOCK, &mask, &omask);

	/*
	 * Check to see if msg looks non-standard.
	 */
	msglen = strlen(msg);
	if (msglen < 16 || msg[3] != ' ' || msg[6] != ' ' ||
	    msg[9] != ':' || msg[12] != ':' || msg[15] != ' ')
		flags |= ADDDATE;

	(void)time(&now);
	if (flags & ADDDATE)
		timestamp = ctime(&now) + 4;
	else {
		timestamp = msg;
		msg += 16;
		msglen -= 16;
	}

	/* extract facility and priority level */
	if (flags & MARK)
		fac = LOG_NFACILITIES;
	else
		fac = LOG_FAC(pri);
	prilev = LOG_PRI(pri);

	/* extract program name */
	for(i = 0; i < NAME_MAX; i++) {
		if (!isalnum(msg[i]) && msg[i] != '-')
			break;
		prog[i] = msg[i];
	}
	prog[i] = 0;

	/* log the message to the particular outputs */
	if (!Initialized) {
		f = &consfile;
		f->f_file = priv_open_tty(ctty);

		if (f->f_file >= 0) {
			fprintlog(f, flags, msg);
			(void)close(f->f_file);
			f->f_file = -1;
		}
		(void)sigprocmask(SIG_SETMASK, &omask, NULL);
		return;
	}
	for (f = Files; f; f = f->f_next) {
		/* skip messages that are incorrect priority */
		if (f->f_pmask[fac] < prilev ||
		    f->f_pmask[fac] == INTERNAL_NOPRI)
			continue;

		/* skip messages with the incorrect program name */
		if (f->f_program)
			if (strcmp(prog, f->f_program) != 0)
				continue;

		if (f->f_type == F_CONSOLE && (flags & IGN_CONS))
			continue;

		/* don't output marks to recently written files */
		if ((flags & MARK) && (now - f->f_time) < MarkInterval / 2)
			continue;

		/*
		 * suppress duplicate lines to this file
		 */
		if ((flags & MARK) == 0 && msglen == f->f_prevlen &&
		    !strcmp(msg, f->f_prevline) &&
		    !strcmp(from, f->f_prevhost)) {
			strlcpy(f->f_lasttime, timestamp, 16);
			f->f_prevcount++;
			dprintf("msg repeated %d times, %ld sec of %d\n",
			    f->f_prevcount, (long)(now - f->f_time),
			    repeatinterval[f->f_repeatcount]);
			/*
			 * If domark would have logged this by now,
			 * flush it now (so we don't hold isolated messages),
			 * but back off so we'll flush less often
			 * in the future.
			 */
			if (now > REPEATTIME(f)) {
				fprintlog(f, flags, (char *)NULL);
				BACKOFF(f);
			}
		} else {
			/* new line, save it */
			if (f->f_prevcount)
				fprintlog(f, 0, (char *)NULL);
			f->f_repeatcount = 0;
			f->f_prevpri = pri;
			strlcpy(f->f_lasttime, timestamp, 16);
			strlcpy(f->f_prevhost, from,
			    sizeof(f->f_prevhost));
			if (msglen < MAXSVLINE) {
				f->f_prevlen = msglen;
				strlcpy(f->f_prevline, msg, sizeof(f->f_prevline));
				fprintlog(f, flags, (char *)NULL);
			} else {
				f->f_prevline[0] = 0;
				f->f_prevlen = 0;
				fprintlog(f, flags, msg);
			}
		}

		if (f->f_quick)
			break;
	}
	(void)sigprocmask(SIG_SETMASK, &omask, NULL);
}

void
fprintlog(struct filed *f, int flags, char *msg)
{
	struct iovec iov[6];
	struct iovec *v;
	int l, retryonce;
	char line[MAXLINE + 1], repbuf[80], greetings[500];

	v = iov;
	if (f->f_type == F_WALL) {
		if ((l = snprintf(greetings, sizeof(greetings),
		    "\r\n\7Message from syslogd@%s at %.24s ...\r\n",
		    f->f_prevhost, ctime(&now))) >= sizeof(greetings) ||
		    l == -1)
			l = strlen(greetings);
		v->iov_base = greetings;
		v->iov_len = l;
		v++;
		v->iov_base = "";
		v->iov_len = 0;
		v++;
	} else {
		v->iov_base = f->f_lasttime;
		v->iov_len = 15;
		v++;
		v->iov_base = " ";
		v->iov_len = 1;
		v++;
	}
	v->iov_base = f->f_prevhost;
	v->iov_len = strlen(v->iov_base);
	v++;
	v->iov_base = " ";
	v->iov_len = 1;
	v++;

	if (msg) {
		v->iov_base = msg;
		v->iov_len = strlen(msg);
	} else if (f->f_prevcount > 1) {
		if ((l = snprintf(repbuf, sizeof(repbuf),
		    "last message repeated %d times", f->f_prevcount)) >=
		    sizeof(repbuf) || l == -1)
			l = strlen(repbuf);
		v->iov_base = repbuf;
		v->iov_len = l;
	} else {
		v->iov_base = f->f_prevline;
		v->iov_len = f->f_prevlen;
	}
	v++;

	dprintf("Logging to %s", TypeNames[f->f_type]);
	f->f_time = now;

	switch (f->f_type) {
	case F_UNUSED:
		dprintf("\n");
		break;

	case F_FORW:
		dprintf(" %s\n", f->f_un.f_forw.f_hname);
		if ((l = snprintf(line, sizeof(line), "<%d>%.15s %s",
		    f->f_prevpri, (char *)iov[0].iov_base,
		    (char *)iov[4].iov_base)) >= sizeof(line) || l == -1)
			l = strlen(line);
		if (sendto(pfd[PFD_INET].fd, line, l, 0,
		    (struct sockaddr *)&f->f_un.f_forw.f_addr,
		    f->f_un.f_forw.f_addr.ss_len) != l) {
			switch (errno) {
			case EHOSTDOWN:
			case EHOSTUNREACH:
			case ENETDOWN:
			case ENOBUFS:
				/* silently dropped */
				break;
			default:
				f->f_type = F_UNUSED;
				logerror("sendto");
				break;
			}
		}
		break;

	case F_CONSOLE:
		if (flags & IGN_CONS) {
			dprintf(" (ignored)\n");
			break;
		}
		/* FALLTHROUGH */

	case F_TTY:
	case F_FILE:
	case F_PIPE:
		dprintf(" %s\n", f->f_un.f_fname);
		if (f->f_type != F_FILE && f->f_type != F_PIPE) {
			v->iov_base = "\r\n";
			v->iov_len = 2;
		} else {
			v->iov_base = "\n";
			v->iov_len = 1;
		}
		retryonce = 0;
	again:
		if (writev(f->f_file, iov, 6) < 0) {
			int e = errno;

			/* pipe is non-blocking. log and drop message if full */
			if (e == EAGAIN && f->f_type == F_PIPE) {
				if (now - f->f_lasterrtime > 120) {
					f->f_lasterrtime = now;
					logerror(f->f_un.f_fname);
				}
				break;
			}

			(void)close(f->f_file);
			/*
			 * Check for errors on TTY's or program pipes.
			 * Errors happen due to loss of tty or died programs.
			 */
			if (e == EAGAIN) {
				/*
				 * Silently drop messages on blocked write.
				 * This can happen when logging to a locked tty.
				 */
				break;
			} else if ((e == EIO || e == EBADF) &&
			    f->f_type != F_FILE && f->f_type != F_PIPE &&
			    !retryonce) {
				f->f_file = priv_open_tty(f->f_un.f_fname);
				retryonce = 1;
				if (f->f_file < 0) {
					f->f_type = F_UNUSED;
					logerror(f->f_un.f_fname);
				} else
					goto again;
			} else if ((e == EPIPE || e == EBADF) &&
			    f->f_type == F_PIPE && !retryonce) {
				f->f_file = priv_open_log(f->f_un.f_fname);
				retryonce = 1;
				if (f->f_file < 0) {
					f->f_type = F_UNUSED;
					logerror(f->f_un.f_fname);
				} else
					goto again;
			} else {
				f->f_type = F_UNUSED;
				f->f_file = -1;
				errno = e;
				logerror(f->f_un.f_fname);
			}
		} else if (flags & SYNC_FILE)
			(void)fsync(f->f_file);
		break;

	case F_USERS:
	case F_WALL:
		dprintf("\n");
		v->iov_base = "\r\n";
		v->iov_len = 2;
		wallmsg(f, iov);
		break;

	case F_MEMBUF:
		dprintf("\n");
		snprintf(line, sizeof(line), "%.15s %s %s",
		    (char *)iov[0].iov_base, (char *)iov[2].iov_base, 
		    (char *)iov[4].iov_base);
		if (ringbuf_append_line(f->f_un.f_mb.f_rb, line) == 1)
			f->f_un.f_mb.f_overflow = 1;
		if (f->f_un.f_mb.f_attached)
			logto_ctlconn(line);
		break;
	}
	f->f_prevcount = 0;
}

/*
 *  WALLMSG -- Write a message to the world at large
 *
 *	Write the specified message to either the entire
 *	world, or a list of approved users.
 */
void
wallmsg(struct filed *f, struct iovec *iov)
{
	struct utmp ut;
	char line[sizeof(ut.ut_line) + 1], *p;
	static int reenter;			/* avoid calling ourselves */
	FILE *uf;
	int i;

	if (reenter++)
		return;
	if ((uf = priv_open_utmp()) == NULL) {
		logerror(_PATH_UTMP);
		reenter = 0;
		return;
	}
	/* NOSTRICT */
	while (fread((char *)&ut, sizeof(ut), 1, uf) == 1) {
		if (ut.ut_name[0] == '\0')
			continue;
		/* must use strncpy since ut_* may not be NUL terminated */
		strncpy(line, ut.ut_line, sizeof(line) - 1);
		line[sizeof(line) - 1] = '\0';
		if (f->f_type == F_WALL) {
			if ((p = ttymsg(iov, 6, line, TTYMSGTIME)) != NULL) {
				errno = 0;	/* already in msg */
				logerror(p);
			}
			continue;
		}
		/* should we send the message to this user? */
		for (i = 0; i < MAXUNAMES; i++) {
			if (!f->f_un.f_uname[i][0])
				break;
			if (!strncmp(f->f_un.f_uname[i], ut.ut_name,
			    UT_NAMESIZE)) {
				if ((p = ttymsg(iov, 6, line, TTYMSGTIME))
								!= NULL) {
					errno = 0;	/* already in msg */
					logerror(p);
				}
				break;
			}
		}
	}
	(void)fclose(uf);
	reenter = 0;
}

/* ARGSUSED */
void
reapchild(int signo)
{
	int save_errno = errno;
	int status;

	while (waitpid(-1, &status, WNOHANG) > 0)
		;
	errno = save_errno;
}

/*
 * Return a printable representation of a host address.
 */
void
cvthname(struct sockaddr_in *f, char *result, size_t res_len)
{
	sigset_t omask, nmask;
	char *p, *ip;
	int ret_len;

	if (f->sin_family != AF_INET) {
		dprintf("Malformed from address\n");
		strlcpy(result, "???", res_len);
		return;
	}

	ip = inet_ntoa(f->sin_addr);
	dprintf("cvthname(%s)\n", ip);
	if (NoDNS) {
		strlcpy(result, ip, res_len);
		return;
	}

	sigemptyset(&nmask);
	sigaddset(&nmask, SIGHUP);
	sigprocmask(SIG_BLOCK, &nmask, &omask);

	ret_len = priv_gethostbyaddr((char *)&f->sin_addr,
		sizeof(struct in_addr), f->sin_family, result, res_len);

	sigprocmask(SIG_SETMASK, &omask, NULL);
	if (ret_len == 0) {
		dprintf("Host name for your address (%s) unknown\n", ip);
		strlcpy(result, ip, res_len);
	} else if ((p = strchr(result, '.')) && strcmp(p + 1, LocalDomain) == 0)
		*p = '\0';
}

void
dodie(int signo)
{
	WantDie = signo;
}

/* ARGSUSED */
void
domark(int signo)
{
	MarkSet = 1;
}

/* ARGSUSED */
void
doinit(int signo)
{
	DoInit = 1;
}

/*
 * Print syslogd errors some place.
 */
void
logerror(const char *type)
{
	char buf[100];

	if (errno)
		(void)snprintf(buf, sizeof(buf), "syslogd: %s: %s",
		    type, strerror(errno));
	else
		(void)snprintf(buf, sizeof(buf), "syslogd: %s", type);
	errno = 0;
	dprintf("%s\n", buf);
	if (Startup)
		fprintf(stderr, "%s\n", buf);
	else
		logmsg(LOG_SYSLOG|LOG_ERR, buf, LocalHostName, ADDDATE);
}

void
die(int signo)
{
	struct filed *f;
	int was_initialized = Initialized;
	char buf[100];

	Initialized = 0;		/* Don't log SIGCHLDs */
	alarm(0);
	for (f = Files; f != NULL; f = f->f_next) {
		/* flush any pending output */
		if (f->f_prevcount)
			fprintlog(f, 0, (char *)NULL);
	}
	Initialized = was_initialized;
	if (signo) {
		dprintf("syslogd: exiting on signal %d\n", signo);
		(void)snprintf(buf, sizeof buf, "exiting on signal %d", signo);
		errno = 0;
		logerror(buf);
	}
	dprintf("[unpriv] syslogd child about to exit\n");
	exit(0);
}

/*
 *  INIT -- Initialize syslogd from configuration table
 */
void
init(void)
{
	char cline[LINE_MAX], prog[NAME_MAX+1], *p;
	struct filed *f, *next, **nextp, *mb, *m;
	FILE *cf;
	int i;

	dprintf("init\n");

	/* If config file has been modified, then just die to restart */
	if (priv_config_modified()) {
		dprintf("config file changed: dying\n");
		die(0);
	}

	/*
	 *  Close all open log files.
	 */
	Initialized = 0;
	mb = NULL;
	for (f = Files; f != NULL; f = next) {
		/* flush any pending output */
		if (f->f_prevcount)
			fprintlog(f, 0, (char *)NULL);

		switch (f->f_type) {
		case F_FILE:
		case F_TTY:
		case F_CONSOLE:
		case F_PIPE:
			(void)close(f->f_file);
			break;
		case F_FORW:
			break;
		}
		next = f->f_next;
		if (f->f_program)
			free(f->f_program);
		if (f->f_type == F_MEMBUF) {
			f->f_next = mb;
			f->f_program = NULL;
			dprintf("add %p to mb: %p\n", f, mb);
			mb = f;
		} else
			free((char *)f);
	}
	Files = NULL;
	nextp = &Files;

	/* open the configuration file */
	if ((cf = priv_open_config()) == NULL) {
		dprintf("cannot open %s\n", ConfFile);
		*nextp = cfline("*.ERR\t/dev/console", "*");
		(*nextp)->f_next = cfline("*.PANIC\t*", "*");
		Initialized = 1;
		return;
	}

	/*
	 *  Foreach line in the conf table, open that file.
	 */
	f = NULL;
	strlcpy(prog, "*", sizeof(prog));
	while (fgets(cline, sizeof(cline), cf) != NULL) {
		/*
		 * check for end-of-section, comments, strip off trailing
		 * spaces and newline character. !prog is treated
		 * specially: the following lines apply only to that program.
		 */
		for (p = cline; isspace(*p); ++p)
			continue;
		if (*p == '\0' || *p == '#')
			continue;
		if (*p == '!') {
			p++;
			while (isspace(*p))
				p++;
			if (!*p || (*p == '*' && (!p[1] || isspace(p[1])))) {
				strlcpy(prog, "*", sizeof(prog));
				continue;
			}
			for (i = 0; i < NAME_MAX; i++) {
				if (!isalnum(p[i]) && p[i] != '-' && p[i] != '!')
					break;
				prog[i] = p[i];
			}
			prog[i] = 0;
			continue;
		}
		p = cline + strlen(cline);
		while (p > cline)
			if (!isspace(*--p)) {
				p++;
				break;
			}
		*p = '\0';
		f = cfline(cline, prog);
		if (f != NULL) {
			*nextp = f;
			nextp = &f->f_next;
		}
	}

	/* Match and initialize the memory buffers */
	for (f = Files; f != NULL; f = f->f_next) {
		if (f->f_type != F_MEMBUF)
			continue;
		dprintf("Initialize membuf %s at %p\n", f->f_un.f_mb.f_mname, f);

		for (m = mb; m != NULL; m = m->f_next) {
			if (m->f_un.f_mb.f_rb == NULL)
				continue;
			if (strcmp(m->f_un.f_mb.f_mname,
			    f->f_un.f_mb.f_mname) == 0)
				break;
		}
		if (m == NULL) {
			dprintf("Membuf no match\n");
			f->f_un.f_mb.f_rb = ringbuf_init(f->f_un.f_mb.f_len);
			if (f->f_un.f_mb.f_rb == NULL) {
				f->f_type = F_UNUSED;
				logerror("Failed to allocate membuf");
			}
		} else {
			dprintf("Membuf match f:%p, m:%p\n", f, m);
			f->f_un = m->f_un;
			m->f_un.f_mb.f_rb = NULL;
		}
	}

	/* make sure remaining buffers are freed */
	while (mb != NULL) {
		m = mb;
		if (m->f_un.f_mb.f_rb != NULL) {
			logerror("Mismatched membuf");
			ringbuf_free(m->f_un.f_mb.f_rb);
		}
		dprintf("Freeing membuf %p\n", m);

		mb = m->f_next;
		free(m);
	}

	/* close the configuration file */
	(void)fclose(cf);

	Initialized = 1;

	if (Debug) {
		for (f = Files; f; f = f->f_next) {
			for (i = 0; i <= LOG_NFACILITIES; i++)
				if (f->f_pmask[i] == INTERNAL_NOPRI)
					printf("X ");
				else
					printf("%d ", f->f_pmask[i]);
			printf("%s: ", TypeNames[f->f_type]);
			switch (f->f_type) {
			case F_FILE:
			case F_TTY:
			case F_CONSOLE:
			case F_PIPE:
				printf("%s", f->f_un.f_fname);
				break;

			case F_FORW:
				printf("%s", f->f_un.f_forw.f_hname);
				break;

			case F_USERS:
				for (i = 0; i < MAXUNAMES && *f->f_un.f_uname[i]; i++)
					printf("%s, ", f->f_un.f_uname[i]);
				break;

			case F_MEMBUF:
				printf("%s", f->f_un.f_mb.f_mname);
				break;

			}
			if (f->f_program)
				printf(" (%s)", f->f_program);
			printf("\n");
		}
	}
}

#define progmatches(p1, p2) \
	(p1 == p2 || (p1 != NULL && p2 != NULL && strcmp(p1, p2) == 0))

/*
 * Spot a line with a duplicate file, pipe, console, tty, or membuf target.
 */
struct filed *
find_dup(struct filed *f)
{
	struct filed *list;

	for (list = Files; list; list = list->f_next) {
		if (list->f_quick || f->f_quick)
			continue;
		switch (list->f_type) {
		case F_FILE:
		case F_TTY:
		case F_CONSOLE:
		case F_PIPE:
			if (strcmp(list->f_un.f_fname, f->f_un.f_fname) == 0 &&
			    progmatches(list->f_program, f->f_program))
				return (list);
			break;
		case F_MEMBUF:
			if (strcmp(list->f_un.f_mb.f_mname,
			    f->f_un.f_mb.f_mname) == 0 &&
			    progmatches(list->f_program, f->f_program))
				return (list);
			break;
		}
	}
	return (NULL);
}

/*
 * Crack a configuration file line
 */
struct filed *
cfline(char *line, char *prog)
{
	int i, pri, addr_len;
	size_t rb_len;
	char *bp, *p, *q, *cp;
	char buf[MAXLINE], ebuf[100];
	struct filed *xf, *f, *d;

	dprintf("cfline(\"%s\", f, \"%s\")\n", line, prog);

	errno = 0;	/* keep strerror() stuff out of logerror messages */

	if ((f = calloc(1, sizeof(*f))) == NULL) {
		logerror("Couldn't allocate struct filed");
		die(0);
	}
	for (i = 0; i <= LOG_NFACILITIES; i++)
		f->f_pmask[i] = INTERNAL_NOPRI;

	/* save program name if any */
	if (*prog == '!') {
		f->f_quick = 1;
		prog++;
	} else
		f->f_quick = 0;
	if (!strcmp(prog, "*"))
		prog = NULL;
	else {
		f->f_program = calloc(1, strlen(prog)+1);
		if (f->f_program)
			strlcpy(f->f_program, prog, strlen(prog)+1);
	}

	/* scan through the list of selectors */
	for (p = line; *p && *p != '\t';) {

		/* find the end of this facility name list */
		for (q = p; *q && *q != '\t' && *q++ != '.'; )
			continue;

		/* collect priority name */
		for (bp = buf; *q && !strchr("\t,;", *q); )
			*bp++ = *q++;
		*bp = '\0';

		/* skip cruft */
		while (*q && strchr(", ;", *q))
			q++;

		/* decode priority name */
		if (*buf == '*')
			pri = LOG_PRIMASK + 1;
		else {
			/* ignore trailing spaces */
			for (i=strlen(buf)-1; i >= 0 && buf[i] == ' '; i--) {
				buf[i]='\0';
			}

			pri = decode(buf, prioritynames);
			if (pri < 0) {
				(void)snprintf(ebuf, sizeof ebuf,
				    "unknown priority name \"%s\"", buf);
				logerror(ebuf);
				free(f);
				return (NULL);
			}
		}

		/* scan facilities */
		while (*p && !strchr("\t.;", *p)) {
			for (bp = buf; *p && !strchr("\t,;.", *p); )
				*bp++ = *p++;
			*bp = '\0';
			if (*buf == '*')
				for (i = 0; i < LOG_NFACILITIES; i++)
					f->f_pmask[i] = pri;
			else {
				i = decode(buf, facilitynames);
				if (i < 0) {
					(void)snprintf(ebuf, sizeof(ebuf),
					    "unknown facility name \"%s\"",
					    buf);
					logerror(ebuf);
					free(f);
					return (NULL);
				}
				f->f_pmask[i >> 3] = pri;
			}
			while (*p == ',' || *p == ' ')
				p++;
		}

		p = q;
	}

	/* skip to action part */
	while (*p == '\t')
		p++;

	switch (*p) {
	case '@':
		if (!InetInuse)
			break;
		if ((cp = strrchr(++p, ':')) != NULL)
			*cp++ = '\0';
		if ((strlcpy(f->f_un.f_forw.f_hname, p,
		    sizeof(f->f_un.f_forw.f_hname)) >=
		    sizeof(f->f_un.f_forw.f_hname))) {
			snprintf(ebuf, sizeof(ebuf), "hostname too long \"%s\"",
			    p);
			logerror(ebuf);
			break;
		}
		addr_len = priv_gethostserv(f->f_un.f_forw.f_hname, 
		    cp == NULL ? "syslog" : cp, 
		    (struct sockaddr*)&f->f_un.f_forw.f_addr, 
		    sizeof(f->f_un.f_forw.f_addr));
		if (addr_len < 1) {
			snprintf(ebuf, sizeof(ebuf), "bad hostname \"%s\"", p);
			logerror(ebuf);
			break;
		}
		f->f_type = F_FORW;
		break;

	case '/':
	case '|':
		(void)strlcpy(f->f_un.f_fname, p, sizeof(f->f_un.f_fname));
		d = find_dup(f);
		if (d != NULL) {
			for (i = 0; i <= LOG_NFACILITIES; i++)
				if (f->f_pmask[i] != INTERNAL_NOPRI)
					d->f_pmask[i] = f->f_pmask[i];
			free(f);
			return (NULL);
		}
		if (strcmp(p, ctty) == 0)
			f->f_file = priv_open_tty(p);
		else
			f->f_file = priv_open_log(p);
		if (f->f_file < 0) {
			f->f_type = F_UNUSED;
			logerror(p);
			break;
		}
		if (isatty(f->f_file)) {
			if (strcmp(p, ctty) == 0)
				f->f_type = F_CONSOLE;
			else
				f->f_type = F_TTY;
		} else {
			if (*p == '|')
				f->f_type = F_PIPE;
			else {
				f->f_type = F_FILE;

				/* Clear O_NONBLOCK flag on f->f_file */
				if ((i = fcntl(f->f_file, F_GETFL, 0)) != -1) {
					i &= ~O_NONBLOCK;
					fcntl(f->f_file, F_SETFL, i);
				}
			}
		}
		break;

	case '*':
		f->f_type = F_WALL;
		break;

	case ':':
		f->f_type = F_MEMBUF;

		/* Parse buffer size (in kb) */
		errno = 0;
		rb_len = strtoul(++p, &q, 0);
		if (*p == '\0' || (errno == ERANGE && rb_len == ULONG_MAX) ||
		    *q != ':' || rb_len == 0) {
			f->f_type = F_UNUSED;
			logerror(p);
			break;
		}
		q++;
		rb_len *= 1024;

		/* Copy buffer name */
		for(i = 0; i < sizeof(f->f_un.f_mb.f_mname) - 1; i++) {
			if (!isalnum(q[i]))
				break;
			f->f_un.f_mb.f_mname[i] = q[i];
		}

		/* Make sure buffer name is unique */
		xf = find_dup(f);

		/* Error on missing or non-unique name, or bad buffer length */
		if (i == 0 || rb_len > MAX_MEMBUF || xf != NULL) {
			f->f_type = F_UNUSED;
			logerror(p);
			break;
		}

		/* Set buffer length */
		rb_len = MAX(rb_len, MIN_MEMBUF);
		f->f_un.f_mb.f_len = rb_len;
		f->f_un.f_mb.f_overflow = 0;
		f->f_un.f_mb.f_attached = 0;
		break;

	default:
		for (i = 0; i < MAXUNAMES && *p; i++) {
			for (q = p; *q && *q != ','; )
				q++;
			(void)strncpy(f->f_un.f_uname[i], p, UT_NAMESIZE);
			if ((q - p) > UT_NAMESIZE)
				f->f_un.f_uname[i][UT_NAMESIZE] = '\0';
			else
				f->f_un.f_uname[i][q - p] = '\0';
			while (*q == ',' || *q == ' ')
				q++;
			p = q;
		}
		f->f_type = F_USERS;
		break;
	}
	return (f);
}


/*
 * Retrieve the size of the kernel message buffer, via sysctl.
 */
int
getmsgbufsize(void)
{
	int msgbufsize, mib[2];
	size_t size;

	mib[0] = CTL_KERN;
	mib[1] = KERN_MSGBUFSIZE;
	size = sizeof msgbufsize;
	if (sysctl(mib, 2, &msgbufsize, &size, NULL, 0) == -1) {
		dprintf("couldn't get kern.msgbufsize\n");
		return (0);
	}
	return (msgbufsize);
}

/*
 *  Decode a symbolic name to a numeric value
 */
int
decode(const char *name, const CODE *codetab)
{
	const CODE *c;
	char *p, buf[40];

	if (isdigit(*name))
		return (atoi(name));

	for (p = buf; *name && p < &buf[sizeof(buf) - 1]; p++, name++) {
		if (isupper(*name))
			*p = tolower(*name);
		else
			*p = *name;
	}
	*p = '\0';
	for (c = codetab; c->c_name; c++)
		if (!strcmp(buf, c->c_name))
			return (c->c_val);

	return (-1);
}

void
markit(void)
{
	struct filed *f;

	now = time((time_t *)NULL);
	MarkSeq += TIMERINTVL;
	if (MarkSeq >= MarkInterval) {
		logmsg(LOG_INFO, "-- MARK --",
		    LocalHostName, ADDDATE|MARK);
		MarkSeq = 0;
	}

	for (f = Files; f; f = f->f_next) {
		if (f->f_prevcount && now >= REPEATTIME(f)) {
			dprintf("flush %s: repeated %d times, %d sec.\n",
			    TypeNames[f->f_type], f->f_prevcount,
			    repeatinterval[f->f_repeatcount]);
			fprintlog(f, 0, (char *)NULL);
			BACKOFF(f);
		}
	}
	MarkSet = 0;
	(void)alarm(TIMERINTVL);
}

int
unix_socket(char *path, int type, mode_t mode)
{
	struct sockaddr_un s_un;
	char errbuf[512];
	int fd;
	mode_t old_umask;

	memset(&s_un, 0, sizeof(s_un));
	s_un.sun_family = AF_UNIX;
	if (strlcpy(s_un.sun_path, path, sizeof(s_un.sun_path)) >
	    sizeof(s_un.sun_path)) {
		snprintf(errbuf, sizeof(errbuf), "socket path too long: %s",
		    path);
		logerror(errbuf);
		die(0);
	}

	if ((fd = socket(AF_UNIX, type, 0)) == -1) {
		logerror("socket");
		return (-1);
	}

	if (Debug) {
		if (connect(fd, (struct sockaddr *)&s_un, sizeof(s_un)) == 0 ||
		    errno == EPROTOTYPE) {
			close(fd);
			errno = EISCONN;
			logerror("connect");
			return (-1);
		}
	}

	old_umask = umask(0177);

	unlink(path);
	if (bind(fd, (struct sockaddr *)&s_un, SUN_LEN(&s_un)) == -1) {
		snprintf(errbuf, sizeof(errbuf), "cannot bind %s", path);
		logerror(errbuf);
		umask(old_umask);
		close(fd);
		return (-1);
	}

	umask(old_umask);

	if (chmod(path, mode) == -1) {
		snprintf(errbuf, sizeof(errbuf), "cannot chmod %s", path);
		logerror(errbuf);
		close(fd);
		unlink(path);
		return (-1);
	}

	return (fd);
}

void
double_rbuf(int fd)
{
	socklen_t slen, len;

	slen = sizeof(len);
	if (getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &len, &slen) == 0) {
		len *= 2;
		setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &len, slen);
	}
}

static void
ctlconn_cleanup(void)
{
	struct filed *f;

	if (pfd[PFD_CTLCONN].fd != -1)
		close(pfd[PFD_CTLCONN].fd);

	pfd[PFD_CTLCONN].fd = -1;
	pfd[PFD_CTLCONN].events = pfd[PFD_CTLCONN].revents = 0;

	pfd[PFD_CTLSOCK].events = POLLIN;

	if (ctl_state == CTL_WRITING_CONT_REPLY)
		for (f = Files; f != NULL; f = f->f_next)
			if (f->f_type == F_MEMBUF)
				f->f_un.f_mb.f_attached = 0;

	ctl_state = ctl_cmd_bytes = ctl_reply_offset = ctl_reply_size = 0;
}

void
ctlsock_accept_handler(void)
{
	int fd, flags;

	dprintf("Accepting control connection\n");
	fd = accept(pfd[PFD_CTLSOCK].fd, NULL, NULL);
	if (fd == -1) {
		if (errno != EINTR && errno != ECONNABORTED)
			logerror("accept ctlsock");
		return;
	}

	ctlconn_cleanup();

	/* Only one connection at a time */
	pfd[PFD_CTLSOCK].events = pfd[PFD_CTLSOCK].revents = 0;

	if ((flags = fcntl(fd, F_GETFL)) == -1 ||
	    fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
		logerror("fcntl ctlconn");
		close(fd);
		return;
	}

	pfd[PFD_CTLCONN].fd = fd;
	pfd[PFD_CTLCONN].events = POLLIN;
	ctl_state = CTL_READING_CMD;
	ctl_cmd_bytes = 0;
}

static struct filed
*find_membuf_log(const char *name)
{
	struct filed *f;

	for (f = Files; f != NULL; f = f->f_next) {
		if (f->f_type == F_MEMBUF &&
		    strcmp(f->f_un.f_mb.f_mname, name) == 0)
			break;
	}
	return (f);
}

void
ctlconn_read_handler(void)
{
	ssize_t n;
	struct filed *f;
	u_int32_t flags = 0;
	struct ctl_reply_hdr *reply_hdr = (struct ctl_reply_hdr *)ctl_reply;

	if (ctl_state == CTL_WRITING_REPLY ||
	    ctl_state == CTL_WRITING_CONT_REPLY) {
		/* client has closed the connection */
		ctlconn_cleanup();
		return;
	}

 retry:
	n = read(pfd[PFD_CTLCONN].fd, (char*)&ctl_cmd + ctl_cmd_bytes,
	    sizeof(ctl_cmd) - ctl_cmd_bytes);
	switch (n) {
	case -1:
		if (errno == EINTR)
			goto retry;
		logerror("ctlconn read");
		/* FALLTHROUGH */
	case 0:
		ctlconn_cleanup();
		return;
	default:
		ctl_cmd_bytes += n;
	}

	if (ctl_cmd_bytes < sizeof(ctl_cmd))
		return;

	if (ntohl(ctl_cmd.version) != CTL_VERSION) {
		logerror("Unknown client protocol version");
		ctlconn_cleanup();
		return;
	}

	/* Ensure that logname is \0 terminated */
	if (memchr(ctl_cmd.logname, '\0', sizeof(ctl_cmd.logname)) == NULL) {
		logerror("Corrupt ctlsock command");
		ctlconn_cleanup();
		return;
	}

	*reply_text = '\0';

	ctl_reply_size = ctl_reply_offset = 0;
	memset(reply_hdr, '\0', sizeof(*reply_hdr));

	ctl_cmd.cmd = ntohl(ctl_cmd.cmd);
	dprintf("ctlcmd %x logname \"%s\"\n", ctl_cmd.cmd, ctl_cmd.logname);

	switch (ctl_cmd.cmd) {
	case CMD_READ:
	case CMD_READ_CLEAR:
	case CMD_READ_CONT:
	case CMD_FLAGS:
		f = find_membuf_log(ctl_cmd.logname);
		if (f == NULL) {
			strlcpy(reply_text, "No such log\n", MAX_MEMBUF);
		} else {
			if (ctl_cmd.cmd != CMD_FLAGS) {
				ringbuf_to_string(reply_text, MAX_MEMBUF,
				    f->f_un.f_mb.f_rb);
			}
			if (f->f_un.f_mb.f_overflow)
				flags |= CTL_HDR_FLAG_OVERFLOW;
			if (ctl_cmd.cmd == CMD_READ_CLEAR) {
				ringbuf_clear(f->f_un.f_mb.f_rb);
				f->f_un.f_mb.f_overflow = 0;
			}
			if (ctl_cmd.cmd == CMD_READ_CONT) {
				f->f_un.f_mb.f_attached = 1;
				tailify_replytext(reply_text, 10);
			}
		}
		break;
	case CMD_CLEAR:
		f = find_membuf_log(ctl_cmd.logname);
		if (f == NULL) {
			strlcpy(reply_text, "No such log\n", MAX_MEMBUF);
		} else {
			ringbuf_clear(f->f_un.f_mb.f_rb);
			if (f->f_un.f_mb.f_overflow)
				flags |= CTL_HDR_FLAG_OVERFLOW;
			f->f_un.f_mb.f_overflow = 0;
			strlcpy(reply_text, "Log cleared\n", MAX_MEMBUF);
		}
		break;
	case CMD_LIST:
		for (f = Files; f != NULL; f = f->f_next) {
			if (f->f_type == F_MEMBUF) {
				strlcat(reply_text, f->f_un.f_mb.f_mname,
				    MAX_MEMBUF);
				if (f->f_un.f_mb.f_overflow) {
					strlcat(reply_text, "*", MAX_MEMBUF);
					flags |= CTL_HDR_FLAG_OVERFLOW;
				}
				strlcat(reply_text, " ", MAX_MEMBUF);
			}
		}
		strlcat(reply_text, "\n", MAX_MEMBUF);
		break;
	default:
		logerror("Unsupported ctlsock command");
		ctlconn_cleanup();
		return;
	}
	reply_hdr->version = htonl(CTL_VERSION);
	reply_hdr->flags = htonl(flags);

	ctl_reply_size = CTL_REPLY_SIZE;
	dprintf("ctlcmd reply length %lu\n", (u_long)ctl_reply_size);

	/* Otherwise, set up to write out reply */
	ctl_state = (ctl_cmd.cmd == CMD_READ_CONT) ?
	    CTL_WRITING_CONT_REPLY : CTL_WRITING_REPLY;

	pfd[PFD_CTLCONN].events = POLLOUT;

	/* monitor terminating syslogc */
	pfd[PFD_CTLCONN].events |= POLLIN;

	/* another syslogc can kick us out */
	if (ctl_state == CTL_WRITING_CONT_REPLY)
		pfd[PFD_CTLSOCK].events = POLLIN;

}

void
ctlconn_write_handler(void)
{
	ssize_t n;

	if (!(ctl_state == CTL_WRITING_REPLY ||
	    ctl_state == CTL_WRITING_CONT_REPLY)) {
		/* Shouldn't be here! */
		logerror("ctlconn_write with bad ctl_state");
		ctlconn_cleanup();
		return;
	}
 retry:
	n = write(pfd[PFD_CTLCONN].fd, ctl_reply + ctl_reply_offset,
	    ctl_reply_size - ctl_reply_offset);
	switch (n) {
	case -1:
		if (errno == EINTR)
			goto retry;
		if (errno != EPIPE)
			logerror("ctlconn write");
		/* FALLTHROUGH */
	case 0:
		ctlconn_cleanup();
		return;
	default:
		ctl_reply_offset += n;
	}
	if (ctl_reply_offset >= ctl_reply_size) {
		/*
		 * Make space in the buffer for continous writes.
		 * Set offset behind reply header to skip it
		 */
		if (ctl_state == CTL_WRITING_CONT_REPLY) {
			*reply_text = '\0';
			ctl_reply_offset = ctl_reply_size = CTL_REPLY_SIZE;

			/* Now is a good time to report dropped lines */
			if (membuf_drop) {
				strlcat(reply_text, "<ENOBUFS>\n", MAX_MEMBUF);
				ctl_reply_size = CTL_REPLY_SIZE;
				membuf_drop = 0;
			} else {
				/* Nothing left to write */
				pfd[PFD_CTLCONN].events = POLLIN;
			}
		} else
			ctlconn_cleanup();
	}
}

/* Shorten replytext to number of lines */
void
tailify_replytext(char *replytext, int lines)
{
	char *start, *nl;
	int count = 0;
	start = nl = replytext;

	while ((nl = strchr(nl, '\n')) != NULL) {
		nl++;
		if (++count > lines) {
			start = strchr(start, '\n');
			start++;
		}
	}
	if (start != replytext) {
		int len = strlen(start);
		memmove(replytext, start, len);
		*(replytext + len) = '\0';
	}
}

void
logto_ctlconn(char *line)
{
	size_t l;

	if (membuf_drop)
		return;

	l = strlen(line);
	if (l + 2 > (CTL_REPLY_MAXSIZE - ctl_reply_size)) {
		/* remember line drops for later report */
		membuf_drop = 1;
		return;
	}
	memcpy(ctl_reply + ctl_reply_size, line, l);
	memcpy(ctl_reply + ctl_reply_size + l, "\n", 2);
	ctl_reply_size += l + 1;
	pfd[PFD_CTLCONN].events |= POLLOUT;
}
