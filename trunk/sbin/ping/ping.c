/*	$OpenBSD: ping.c,v 1.78 2007/02/06 15:25:18 jmc Exp $	*/
/*	$NetBSD: ping.c,v 1.20 1995/08/11 22:37:58 cgd Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mike Muuss.
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
"@(#) Copyright (c) 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)ping.c	8.1 (Berkeley) 6/5/93";
#else
static const char rcsid[] = "$OpenBSD: ping.c,v 1.78 2007/02/06 15:25:18 jmc Exp $";
#endif
#endif /* not lint */

/*
 *			P I N G . C
 *
 * Using the InterNet Control Message Protocol (ICMP) "ECHO" facility,
 * measure round-trip-delays and packet loss across network paths.
 *
 * Author -
 *	Mike Muuss
 *	U. S. Army Ballistic Research Laboratory
 *	December, 1983
 *
 * Status -
 *	Public Domain.  Distribution Unlimited.
 * Bugs -
 *	More statistics could always be gathered.
 *	This program has to run SUID to ROOT to access the ICMP socket.
 */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/time.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_var.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

struct tvi {
	u_int	tv_sec;
	u_int	tv_usec;
};

#define	DEFDATALEN	(64 - 8)		/* default data length */
#define	MAXIPLEN	60
#define	MAXICMPLEN	76
#define	MAXPAYLOAD	(IP_MAXPACKET - MAXIPLEN - 8) /* max ICMP payload size */
#define	MAXWAIT_DEFAULT	10			/* secs to wait for response */
#define	NROUTES		9			/* number of record route slots */

#define	A(bit)		rcvd_tbl[(bit)>>3]	/* identify byte in array */
#define	B(bit)		(1 << ((bit) & 0x07))	/* identify bit in byte */
#define	SET(bit)	(A(bit) |= B(bit))
#define	CLR(bit)	(A(bit) &= (~B(bit)))
#define	TST(bit)	(A(bit) & B(bit))

/* various options */
int options;
#define	F_FLOOD		0x0001
#define	F_INTERVAL	0x0002
#define	F_NUMERIC	0x0004
#define	F_PINGFILLED	0x0008
#define	F_QUIET		0x0010
#define	F_RROUTE	0x0020
#define	F_SO_DEBUG	0x0040
#define	F_SO_DONTROUTE	0x0080
#define	F_VERBOSE	0x0100
#define	F_SADDR		0x0200
#define	F_HDRINCL	0x0400
#define	F_TTL		0x0800
#define	F_SO_JUMBO	0x1000

/* multicast options */
int moptions;
#define	MULTICAST_NOLOOP	0x001
#define	MULTICAST_TTL		0x002
#define	MULTICAST_IF		0x004

/*
 * MAX_DUP_CHK is the number of bits in received table, i.e. the maximum
 * number of received sequence numbers we can keep track of.  Change 128
 * to 8192 for complete accuracy...
 */
#define	MAX_DUP_CHK	(8 * 128)
int mx_dup_ck = MAX_DUP_CHK;
char rcvd_tbl[MAX_DUP_CHK / 8];

struct sockaddr_in whereto;	/* who to ping */
struct sockaddr_in whence;		/* Which interface we come from */
unsigned int datalen = DEFDATALEN;
int s;				/* socket file descriptor */
u_char outpackhdr[IP_MAXPACKET]; /* Max packet size = 65535 */
u_char *outpack = outpackhdr+sizeof(struct ip);
char BSPACE = '\b';		/* characters written for flood */
char DOT = '.';
char *hostname;
int ident;			/* process id to identify our packets */

/* counters */
unsigned long npackets;		/* max packets to transmit */
unsigned long nreceived;	/* # of packets we got back */
unsigned long nrepeats;		/* number of duplicates */
unsigned long ntransmitted;	/* sequence # for outbound packets = #sent */
double interval = 1;		/* interval between packets */
struct itimerval interstr;	/* interval structure for use with setitimer */

/* timing */
int timing;			/* flag to do timing */
unsigned int maxwait = MAXWAIT_DEFAULT;	/* max seconds to wait for response */
quad_t tmin = 999999999;	/* minimum round trip time in usec */
quad_t tmax = 0;		/* maximum round trip time in usec */
quad_t tsum = 0;		/* sum of all times in usec, for doing average */
quad_t tsumsq = 0;		/* sum of all times squared, for std. dev. */

int bufspace = IP_MAXPACKET;

void fill(char *, char *);
void catcher(int signo);
void prtsig(int signo);
void finish(int signo);
void summary(int, int);
int in_cksum(u_short *, int);
void pinger(void);
char *pr_addr(in_addr_t);
int check_icmph(struct ip *);
void pr_icmph(struct icmp *);
void pr_pack(char *, int, struct sockaddr_in *);
void pr_retip(struct ip *);
quad_t qsqrt(quad_t);
void pr_iph(struct ip *);
void usage(void);

int
main(int argc, char *argv[])
{
	struct timeval timeout;
	struct hostent *hp;
	struct sockaddr_in *to;
	struct in_addr saddr;
	int i, ch, hold = 1, packlen, preload, maxsize, df = 0, tos = 0;
	u_char *datap, *packet, ttl = MAXTTL, loop = 1;
	char *target, hnamebuf[MAXHOSTNAMELEN];
#ifdef IP_OPTIONS
	char rspace[3 + 4 * NROUTES + 1];	/* record route space */
#endif
	socklen_t maxsizelen;
	const char *errstr;
	fd_set *fdmaskp;
	size_t fdmasks;
	uid_t uid;

	if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0)
		err(1, "socket");

	/* revoke privs */
	uid = getuid();
	if (setresuid(uid, uid, uid) == -1)
		err(1, "setresuid");

	preload = 0;
	datap = &outpack[8 + sizeof(struct tvi)];
	while ((ch = getopt(argc, argv,
	    "DI:LRS:c:dfi:jl:np:qrs:T:t:vw:")) != -1)
		switch(ch) {
		case 'c':
			npackets = (unsigned long)strtonum(optarg, 1, INT_MAX, &errstr);
			if (errstr)
				errx(1,
				    "number of packets to transmit is %s: %s",
				    errstr, optarg);
			break;
		case 'D':
			options |= F_HDRINCL;
			df = 1;
			break;
		case 'd':
			options |= F_SO_DEBUG;
			break;
		case 'f':
			if (getuid())
				errx(1, "%s", strerror(EPERM));
			options |= F_FLOOD;
			setbuf(stdout, (char *)NULL);
			break;
		case 'I':
		case 'S':	/* deprecated */
			if (inet_aton(optarg, &saddr) == 0) {
				if ((hp = gethostbyname(optarg)) == NULL)
					errx(1, "bad interface address: %s",
					    optarg);
				memcpy(&saddr, hp->h_addr, sizeof(saddr));
			}
			options |= F_SADDR;
			break;
		case 'i':		/* wait between sending packets */
			interval = strtod(optarg, NULL);

			if (interval <= 0 || interval >= INT_MAX)
				errx(1, "bad timing interval: %s", optarg);

			if (interval < 1)
				if (getuid())
					errx(1, "%s: only root may use interval < 1s",
					    strerror(EPERM));

			if (interval < 0.01)
				interval = 0.01;

			options |= F_INTERVAL;
			break;
		case 'j':
			options |= F_SO_JUMBO;
			break;
		case 'L':
			moptions |= MULTICAST_NOLOOP;
			loop = 0;
			break;
		case 'l':
			if (getuid())
				errx(1, "%s", strerror(EPERM));
			preload = (int)strtonum(optarg, 1, INT_MAX, &errstr);
			if (errstr)
				errx(1, "preload value is %s: %s",
				    errstr, optarg);
			break;
		case 'n':
			options |= F_NUMERIC;
			break;
		case 'p':		/* fill buffer with user pattern */
			options |= F_PINGFILLED;
			fill((char *)datap, optarg);
				break;
		case 'q':
			options |= F_QUIET;
			break;
		case 'R':
			options |= F_RROUTE;
			break;
		case 'r':
			options |= F_SO_DONTROUTE;
			break;
		case 's':		/* size of packet to send */
			datalen = (unsigned int)strtonum(optarg, 0, MAXPAYLOAD, &errstr);
			if (errstr)
				errx(1, "packet size is %s: %s",
				    errstr, optarg);
			break;
		case 'T':
			options |= F_HDRINCL;
			tos = (int)strtonum(optarg, 0, 0xff, &errstr);
			if (errstr)
				errx(1, "tos value is %s: %s", errstr, optarg);
			break;
		case 't':
			options |= F_TTL;
			ttl = (u_char)strtonum(optarg, 1, 255, &errstr);
			if (errstr)
				errx(1, "ttl value is %s: %s", errstr, optarg);
			break;
		case 'v':
			options |= F_VERBOSE;
			break;
		case 'w':
			maxwait = (unsigned int)strtonum(optarg, 1, INT_MAX, &errstr);
			if (errstr)
				errx(1, "maxwait value is %s: %s",
				    errstr, optarg);
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	memset(&interstr, 0, sizeof(interstr));

	interstr.it_value.tv_sec = (long) interval;
	interstr.it_value.tv_usec =
		(long) ((interval - interstr.it_value.tv_sec) * 1000000);

	target = *argv;

	memset(&whereto, 0, sizeof(whereto));
	to = &whereto;
	to->sin_len = sizeof(struct sockaddr_in);
	to->sin_family = AF_INET;
	if (inet_aton(target, &to->sin_addr) != 0)
		hostname = target;
	else {
		hp = gethostbyname(target);
		if (!hp)
			errx(1, "unknown host: %s", target);
		to->sin_family = hp->h_addrtype;
		memcpy(&to->sin_addr, hp->h_addr, (size_t)hp->h_length);
		(void)strlcpy(hnamebuf, hp->h_name, sizeof(hnamebuf));
		hostname = hnamebuf;
	}

	if (options & F_FLOOD && options & F_INTERVAL)
		errx(1, "-f and -i options are incompatible");

	if (datalen >= sizeof(struct tvi))	/* can we time transfer */
		timing = 1;
	packlen = datalen + MAXIPLEN + MAXICMPLEN;
	if (!(packet = malloc((size_t)packlen)))
		err(1, "malloc");
	if (!(options & F_PINGFILLED))
		for (i = sizeof(struct tvi); i < datalen; ++i)
			*datap++ = i;

	ident = getpid() & 0xFFFF;

	if (options & F_SADDR) {
		if (IN_MULTICAST(ntohl(to->sin_addr.s_addr)))
			moptions |= MULTICAST_IF;
		else {
			memset(&whence, 0, sizeof(whence));
			whence.sin_len = sizeof(whence);
			whence.sin_family = AF_INET;
			memcpy(&whence.sin_addr.s_addr, &saddr, sizeof(saddr));
			if (bind(s, (struct sockaddr *)&whence,
			    sizeof(whence)) < 0)
				err(1, "bind");
		}
	}

	if (options & F_SO_DEBUG)
		(void)setsockopt(s, SOL_SOCKET, SO_DEBUG, &hold,
		    sizeof(hold));
	if (options & F_SO_DONTROUTE)
		(void)setsockopt(s, SOL_SOCKET, SO_DONTROUTE, &hold,
		    sizeof(hold));
	if (options & F_SO_JUMBO)
		(void)setsockopt(s, SOL_SOCKET, SO_JUMBO, &hold,
		    sizeof(hold));

	if (options & F_TTL) {
		if (IN_MULTICAST(ntohl(to->sin_addr.s_addr)))
			moptions |= MULTICAST_TTL;
		else
			options |= F_HDRINCL;
	}

	if (options & F_RROUTE && options & F_HDRINCL)
		errx(1, "-R option and -D or -T, or -t to unicast destinations"
		    " are incompatible");

	if (options & F_HDRINCL) {
		struct ip *ip = (struct ip *)outpackhdr;

		setsockopt(s, IPPROTO_IP, IP_HDRINCL, &hold, sizeof(hold));
		ip->ip_v = IPVERSION;
		ip->ip_hl = sizeof(struct ip) >> 2;
		ip->ip_tos = tos;
		ip->ip_id = 0;
		ip->ip_off = htons(df ? IP_DF : 0);
		ip->ip_ttl = ttl;
		ip->ip_p = IPPROTO_ICMP;
		if (options & F_SADDR)
			ip->ip_src = saddr;
		else
			ip->ip_src.s_addr = INADDR_ANY;
		ip->ip_dst = to->sin_addr;
	}

	/* record route option */
	if (options & F_RROUTE) {
		if (IN_MULTICAST(ntohl(to->sin_addr.s_addr)))
			errx(1, "record route not valid to multicast destinations");
#ifdef IP_OPTIONS
		memset(rspace, 0, sizeof(rspace));
		rspace[IPOPT_OPTVAL] = IPOPT_RR;
		rspace[IPOPT_OLEN] = sizeof(rspace)-1;
		rspace[IPOPT_OFFSET] = IPOPT_MINOFF;
		if (setsockopt(s, IPPROTO_IP, IP_OPTIONS, rspace,
		    sizeof(rspace)) < 0) {
			perror("ping: record route");
			exit(1);
		}
#else
		errx(1, "record route not available in this implementation");
#endif /* IP_OPTIONS */
	}

	if ((moptions & MULTICAST_NOLOOP) &&
	    setsockopt(s, IPPROTO_IP, IP_MULTICAST_LOOP, &loop,
	    sizeof(loop)) < 0)
		err(1, "setsockopt IP_MULTICAST_LOOP");
	if ((moptions & MULTICAST_TTL) &&
	    setsockopt(s, IPPROTO_IP, IP_MULTICAST_TTL, &ttl,
	    sizeof(ttl)) < 0)
		err(1, "setsockopt IP_MULTICAST_TTL");
	if ((moptions & MULTICAST_IF) &&
	    setsockopt(s, IPPROTO_IP, IP_MULTICAST_IF, &saddr,
	    sizeof(saddr)) < 0)
		err(1, "setsockopt IP_MULTICAST_IF");

	/*
	 * When trying to send large packets, you must increase the
	 * size of both the send and receive buffers...
	 */
	maxsizelen = sizeof maxsize;
	if (getsockopt(s, SOL_SOCKET, SO_SNDBUF, &maxsize, &maxsizelen) < 0)
		err(1, "getsockopt");
	if (maxsize < packlen &&
	    setsockopt(s, SOL_SOCKET, SO_SNDBUF, &packlen, sizeof(maxsize)) < 0)
		err(1, "setsockopt");

	/*
	 * When pinging the broadcast address, you can get a lot of answers.
	 * Doing something so evil is useful if you are trying to stress the
	 * ethernet, or just want to fill the arp cache to get some stuff for
	 * /etc/ethers.
	 */
	while (setsockopt(s, SOL_SOCKET, SO_RCVBUF,
	    (void*)&bufspace, sizeof(bufspace)) < 0) {
		if ((bufspace -= 1024) <= 0)
			err(1, "Cannot set the receive buffer size");
	}
	if (bufspace < IP_MAXPACKET)
		warnx("Could only allocate a receive buffer of %i bytes (default %i)",
		    bufspace, IP_MAXPACKET);

	if (to->sin_family == AF_INET)
		(void)printf("PING %s (%s): %d data bytes\n", hostname,
		    inet_ntoa(*(struct in_addr *)&to->sin_addr.s_addr),
		    datalen);
	else
		(void)printf("PING %s: %d data bytes\n", hostname, datalen);

	(void)signal(SIGINT, finish);
	(void)signal(SIGALRM, catcher);
#ifdef SIGINFO
	(void)signal(SIGINFO, prtsig);
#endif

	while (preload--)		/* fire off them quickies */
		pinger();

	if ((options & F_FLOOD) == 0)
		catcher(0);		/* start things going */

	fdmasks = howmany(s+1, NFDBITS) * sizeof(fd_mask);
	if ((fdmaskp = (fd_set *)malloc(fdmasks)) == NULL)
		err(1, "malloc");

	for (;;) {
		struct sockaddr_in from;
		sigset_t omask, nmask;
		socklen_t fromlen;
		int cc;

		if (options & F_FLOOD) {
			pinger();
			timeout.tv_sec = 0;
			timeout.tv_usec = 10000;
			memset(fdmaskp, 0, fdmasks);
			FD_SET(s, fdmaskp);
			if (select(s + 1, (fd_set *)fdmaskp, (fd_set *)NULL,
			    (fd_set *)NULL, &timeout) < 1)
				continue;
		}
		fromlen = sizeof(from);
		if ((cc = recvfrom(s, packet, packlen, 0,
		    (struct sockaddr *)&from, &fromlen)) < 0) {
			if (errno == EINTR)
				continue;
			perror("ping: recvfrom");
			continue;
		}
		sigemptyset(&nmask);
		sigaddset(&nmask, SIGALRM);
		sigprocmask(SIG_BLOCK, &nmask, &omask);
		pr_pack((char *)packet, cc, &from);
		sigprocmask(SIG_SETMASK, &omask, NULL);
		if (npackets && nreceived >= npackets)
			break;
	}
	free(fdmaskp);
	finish(0);
	/* NOTREACHED */
	exit(0);	/* Make the compiler happy */
}

/*
 * catcher --
 *	This routine causes another PING to be transmitted, and then
 * schedules another SIGALRM for 1 second from now.
 *
 * bug --
 *	Our sense of time will slowly skew (i.e., packets will not be
 * launched exactly at 1-second intervals).  This does not affect the
 * quality of the delay and loss statistics.
 */
/* ARGSUSED */
void
catcher(int signo)
{
	int save_errno = errno;
	unsigned int waittime;

	pinger();
	(void)signal(SIGALRM, catcher);
	if (!npackets || ntransmitted < npackets)
		setitimer(ITIMER_REAL, &interstr, (struct itimerval *)0);
	else {
		if (nreceived) {
			waittime = 2 * tmax / 1000000;
			if (!waittime)
				waittime = 1;
		} else
			waittime = maxwait;
		(void)signal(SIGALRM, finish);
		(void)alarm(waittime);
	}
	errno = save_errno;
}

/*
 * Print statistics when SIGINFO is received.
 */
/* ARGSUSED */
void
prtsig(int signo)
{
	int save_errno = errno;

	summary(0, 1);
	errno = save_errno;
}

/*
 * pinger --
 *	Compose and transmit an ICMP ECHO REQUEST packet.  The IP packet
 * will be added on by the kernel.  The ID field is our UNIX process ID,
 * and the sequence number is an ascending integer.  The first 8 bytes
 * of the data portion are used to hold a UNIX "timeval" struct in VAX
 * byte-order, to compute the round-trip time.
 */
void
pinger(void)
{
	struct icmp *icp;
	char buf[8192];
	int cc, i;
	u_char *packet = outpack;

	icp = (struct icmp *)outpack;
	icp->icmp_type = ICMP_ECHO;
	icp->icmp_code = 0;
	icp->icmp_cksum = 0;
	icp->icmp_seq = htons(ntransmitted);
	icp->icmp_id = ident;			/* ID */

	CLR(ntohs(icp->icmp_seq) % mx_dup_ck);

	if (timing) {
		struct timeval tv;
		struct tvi tvi;

		(void)gettimeofday(&tv, (struct timezone *)NULL);
		tvi.tv_sec = htonl(tv.tv_sec);
		tvi.tv_usec = htonl(tv.tv_usec);
		memcpy(&outpack[8], &tvi, sizeof tvi);
	}

	cc = datalen + 8;			/* skips ICMP portion */

	/* compute ICMP checksum here */
	icp->icmp_cksum = in_cksum((u_short *)icp, cc);

	if (options & F_HDRINCL) {
		struct ip *ip = (struct ip *)outpackhdr;

		packet = (u_char *)ip;
		cc += sizeof(struct ip);
		ip->ip_len = htons(cc);
		ip->ip_sum = in_cksum((u_short *)outpackhdr, cc);
	}

	i = sendto(s, packet, cc, 0, (struct sockaddr *)&whereto,
	    sizeof(whereto));

	if (i < 0 || i != cc)  {
		if (i < 0)
			perror("ping: sendto");
		snprintf(buf, sizeof buf, "ping: wrote %s %d chars, ret=%d\n",
		    hostname, cc, i);
		write(STDOUT_FILENO, buf, strlen(buf));
	}
	if (!(options & F_QUIET) && options & F_FLOOD)
		(void)write(STDOUT_FILENO, &DOT, 1);

	ntransmitted++;
}

/*
 * pr_pack --
 *	Print out the packet, if it came from us.  This logic is necessary
 * because ALL readers of the ICMP socket get a copy of ALL ICMP packets
 * which arrive ('tis only fair).  This permits multiple copies of this
 * program to be run without having intermingled output (or statistics!).
 */
void
pr_pack(char *buf, int cc, struct sockaddr_in *from)
{
	struct icmp *icp;
	in_addr_t l;
	u_int i, j;
	u_char *cp, *dp;
	static int old_rrlen;
	static char old_rr[MAX_IPOPTLEN];
	struct ip *ip, *ip2;
	struct timeval tv, tp;
	char *pkttime;
	quad_t triptime = 0;
	int hlen, hlen2, dupflag;

	(void)gettimeofday(&tv, (struct timezone *)NULL);

	/* Check the IP header */
	ip = (struct ip *)buf;
	hlen = ip->ip_hl << 2;
	if (cc < hlen + ICMP_MINLEN) {
		if (options & F_VERBOSE)
			warnx("packet too short (%d bytes) from %s", cc,
			    inet_ntoa(*(struct in_addr *)&from->sin_addr.s_addr));
		return;
	}

	/* Now the ICMP part */
	cc -= hlen;
	icp = (struct icmp *)(buf + hlen);
	if (icp->icmp_type == ICMP_ECHOREPLY) {
		if (icp->icmp_id != ident)
			return;			/* 'Twas not our ECHO */
		++nreceived;
		if (timing) {
			struct tvi tvi;

#ifndef icmp_data
			pkttime = (char *)&icp->icmp_ip;
#else
			pkttime = (char *)icp->icmp_data;
#endif
			memcpy(&tvi, pkttime, sizeof tvi);
			tp.tv_sec = ntohl(tvi.tv_sec);
			tp.tv_usec = ntohl(tvi.tv_usec);

			timersub(&tv, &tp, &tv);
			triptime = (tv.tv_sec * 1000000) + tv.tv_usec;
			tsum += triptime;
			tsumsq += triptime * triptime;
			if (triptime < tmin)
				tmin = triptime;
			if (triptime > tmax)
				tmax = triptime;
		}

		if (TST(ntohs(icp->icmp_seq) % mx_dup_ck)) {
			++nrepeats;
			--nreceived;
			dupflag = 1;
		} else {
			SET(ntohs(icp->icmp_seq) % mx_dup_ck);
			dupflag = 0;
		}

		if (options & F_QUIET)
			return;

		if (options & F_FLOOD)
			(void)write(STDOUT_FILENO, &BSPACE, 1);
		else {
			(void)printf("%d bytes from %s: icmp_seq=%u", cc,
			    inet_ntoa(*(struct in_addr *)&from->sin_addr.s_addr),
			    ntohs(icp->icmp_seq));
			(void)printf(" ttl=%d", ip->ip_ttl);
			if (timing)
				(void)printf(" time=%d.%03d ms",
				    (int)(triptime / 1000),
				    (int)(triptime % 1000));
			if (dupflag)
				(void)printf(" (DUP!)");
			/* check the data */
			cp = (u_char *)&icp->icmp_data[sizeof(struct tvi)];
			dp = &outpack[8 + sizeof(struct tvi)];
			for (i = 8 + sizeof(struct tvi); i < datalen;
			    ++i, ++cp, ++dp) {
				if (*cp != *dp) {
					(void)printf("\nwrong data byte #%d "
					    "should be 0x%x but was 0x%x",
					    i, *dp, *cp);
					cp = (u_char *)&icp->icmp_data[0];
					for (i = 8; i < datalen; ++i, ++cp) {
						if ((i % 32) == 8)
							(void)printf("\n\t");
						(void)printf("%x ", *cp);
					}
					break;
				}
			}
		}
	} else {
		/* We've got something other than an ECHOREPLY */
		if (!(options & F_VERBOSE))
			return;
		ip2 = (struct ip *)(buf + hlen + sizeof (struct icmp));
		hlen2 = ip2->ip_hl << 2;
		if (cc >= hlen2 + 8 && check_icmph((struct ip *)(icp +
		    sizeof (struct icmp))) != 1)
			return;
		(void)printf("%d bytes from %s: ", cc,
		    pr_addr(from->sin_addr.s_addr));
		pr_icmph(icp);
	}

	/* Display any IP options */
	cp = (u_char *)buf + sizeof(struct ip);

	for (; hlen > (int)sizeof(struct ip); --hlen, ++cp)
		switch (*cp) {
		case IPOPT_EOL:
			hlen = 0;
			break;
		case IPOPT_LSRR:
			(void)printf("\nLSRR: ");
			hlen -= 2;
			j = *++cp;
			++cp;
			i = 0;
			if (j > IPOPT_MINOFF) {
				for (;;) {
					l = *++cp;
					l = (l<<8) + *++cp;
					l = (l<<8) + *++cp;
					l = (l<<8) + *++cp;
					if (l == 0)
						(void)printf("\t0.0.0.0");
					else
						(void)printf("\t%s",
						    pr_addr(ntohl(l)));
					hlen -= 4;
					j -= 4;
					i += 4;
					if (j <= IPOPT_MINOFF)
						break;
					if (i >= MAX_IPOPTLEN) {
						(void)printf("\t(truncated route)");
						break;
					}
					(void)putchar('\n');
				}
			}
			break;
		case IPOPT_RR:
			j = *++cp;		/* get length */
			i = *++cp;		/* and pointer */
			hlen -= 2;
			if (i > j)
				i = j;
			i -= IPOPT_MINOFF;
			if (i <= 0)
				continue;
			if (i == old_rrlen &&
			    cp == (u_char *)buf + sizeof(struct ip) + 2 &&
			    !memcmp(cp, old_rr, i) &&
			    !(options & F_FLOOD)) {
				(void)printf("\t(same route)");
				i = ((i + 3) / 4) * 4;
				hlen -= i;
				cp += i;
				break;
			}
			if (i < MAX_IPOPTLEN) {
				old_rrlen = i;
				memcpy(old_rr, cp, i);
			} else
				old_rrlen = 0;

			(void)printf("\nRR: ");
			j = 0;
			for (;;) {
				l = *++cp;
				l = (l<<8) + *++cp;
				l = (l<<8) + *++cp;
				l = (l<<8) + *++cp;
				if (l == 0)
					(void)printf("\t0.0.0.0");
				else
					(void)printf("\t%s", pr_addr(ntohl(l)));
				hlen -= 4;
				i -= 4;
				j += 4;
				if (i <= 0)
					break;
				if (j >= MAX_IPOPTLEN) {
					(void)printf("\t(truncated route)");
					break;
				}
				(void)putchar('\n');
			}
			break;
		case IPOPT_NOP:
			(void)printf("\nNOP");
			break;
		default:
			(void)printf("\nunknown option %x", *cp);
			hlen = hlen - (cp[IPOPT_OLEN] - 1);
			cp = cp + (cp[IPOPT_OLEN] - 1);
			break;
		}
	if (!(options & F_FLOOD)) {
		(void)putchar('\n');
		(void)fflush(stdout);
	}
}

/*
 * in_cksum --
 *	Checksum routine for Internet Protocol family headers (C Version)
 */
int
in_cksum(u_short *addr, int len)
{
	int nleft = len;
	u_short *w = addr;
	int sum = 0;
	u_short answer = 0;

	/*
	 * Our algorithm is simple, using a 32 bit accumulator (sum), we add
	 * sequential 16 bit words to it, and at the end, fold back all the
	 * carry bits from the top 16 bits into the lower 16 bits.
	 */
	while (nleft > 1)  {
		sum += *w++;
		nleft -= 2;
	}

	/* mop up an odd byte, if necessary */
	if (nleft == 1) {
		*(u_char *)(&answer) = *(u_char *)w ;
		sum += answer;
	}

	/* add back carry outs from top 16 bits to low 16 bits */
	sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
	sum += (sum >> 16);			/* add carry */
	answer = ~sum;				/* truncate to 16 bits */
	return(answer);
}

void
summary(int header, int insig)
{
	char buf[8192], buft[8192];

	buf[0] = '\0';

	if (!insig) {
		(void)putchar('\r');
		(void)fflush(stdout);
	} else
		strlcat(buf, "\r", sizeof buf);

	if (header) {
		snprintf(buft, sizeof buft, "--- %s ping statistics ---\n",
		    hostname);
		strlcat(buf, buft, sizeof buf);
	}

	snprintf(buft, sizeof buft, "%ld packets transmitted, ", ntransmitted);
	strlcat(buf, buft, sizeof buf);
	snprintf(buft, sizeof buft, "%ld packets received, ", nreceived);
	strlcat(buf, buft, sizeof buf);

	if (nrepeats) {
		snprintf(buft, sizeof buft, "%ld duplicates, ", nrepeats);
		strlcat(buf, buft, sizeof buf);
	}
	if (ntransmitted) {
		if (nreceived > ntransmitted)
			snprintf(buft, sizeof buft,
			    "-- somebody's duplicating packets!");
		else
			snprintf(buft, sizeof buft, "%.1f%% packet loss",
			    ((((double)ntransmitted - nreceived) * 100) /
			    ntransmitted));
		strlcat(buf, buft, sizeof buf);
	}
	strlcat(buf, "\n", sizeof buf);
	if (nreceived && timing) {
		quad_t num = nreceived + nrepeats;
		quad_t avg = tsum / num;
		quad_t dev = qsqrt(tsumsq / num - avg * avg);

		snprintf(buft, sizeof buft, "round-trip min/avg/max/std-dev = "
		    "%d.%03d/%d.%03d/%d.%03d/%d.%03d ms\n",
		    (int)(tmin / 1000), (int)(tmin % 1000),
		    (int)(avg  / 1000), (int)(avg  % 1000),
		    (int)(tmax / 1000), (int)(tmax % 1000),
		    (int)(dev  / 1000), (int)(dev  % 1000));
		strlcat(buf, buft, sizeof buf);
	}
	write(STDOUT_FILENO, buf, strlen(buf));		/* XXX atomicio? */
}

quad_t
qsqrt(quad_t qdev)
{
	quad_t y, x = 1;

	if (!qdev)
		return(0);

	do { /* newton was a stinker */
		y = x;
		x = qdev / x;
		x += y;
		x /= 2;
	} while ((x - y) > 1 || (x - y) < -1);

	return(x);
}

/*
 * finish --
 *	Print out statistics, and give up.
 */
void
finish(int signo)
{
	(void)signal(SIGINT, SIG_IGN);

	summary(1, signo);
	if (signo)
		_exit(nreceived ? 0 : 1);
	else
		exit(nreceived ? 0 : 1);
}

#ifdef notdef
static char *ttab[] = {
	"Echo Reply",		/* ip + seq + udata */
	"Dest Unreachable",	/* net, host, proto, port, frag, sr + IP */
	"Source Quench",	/* IP */
	"Redirect",		/* redirect type, gateway, + IP  */
	"Echo",
	"Time Exceeded",	/* transit, frag reassem + IP */
	"Parameter Problem",	/* pointer + IP */
	"Timestamp",		/* id + seq + three timestamps */
	"Timestamp Reply",	/* " */
	"Info Request",		/* id + sq */
	"Info Reply"		/* " */
};
#endif

/*
 * pr_icmph --
 *	Print a descriptive string about an ICMP header.
 */
void
pr_icmph(struct icmp *icp)
{
	switch(icp->icmp_type) {
	case ICMP_ECHOREPLY:
		(void)printf("Echo Reply\n");
		/* XXX ID + Seq + Data */
		break;
	case ICMP_UNREACH:
		switch(icp->icmp_code) {
		case ICMP_UNREACH_NET:
			(void)printf("Destination Net Unreachable\n");
			break;
		case ICMP_UNREACH_HOST:
			(void)printf("Destination Host Unreachable\n");
			break;
		case ICMP_UNREACH_PROTOCOL:
			(void)printf("Destination Protocol Unreachable\n");
			break;
		case ICMP_UNREACH_PORT:
			(void)printf("Destination Port Unreachable\n");
			break;
		case ICMP_UNREACH_NEEDFRAG:
			if (icp->icmp_nextmtu != 0)
				(void)printf("frag needed and DF set (MTU %d)\n",
				    ntohs(icp->icmp_nextmtu));
			else
				(void)printf("frag needed and DF set\n");
			break;
		case ICMP_UNREACH_SRCFAIL:
			(void)printf("Source Route Failed\n");
			break;
		case ICMP_UNREACH_NET_UNKNOWN:
			(void)printf("Network Unknown\n");
			break;
		case ICMP_UNREACH_HOST_UNKNOWN:
			(void)printf("Host Unknown\n");
			break;
		case ICMP_UNREACH_ISOLATED:
			(void)printf("Source Isolated\n");
			break;
		case ICMP_UNREACH_NET_PROHIB:
			(void)printf("Dest. Net Administratively Prohibited\n");
			break;
		case ICMP_UNREACH_HOST_PROHIB:
			(void)printf("Dest. Host Administratively Prohibited\n");
			break;
		case ICMP_UNREACH_TOSNET:
			(void)printf("Destination Net Unreachable for TOS\n");
			break;
		case ICMP_UNREACH_TOSHOST:
			(void)printf("Desination Host Unreachable for TOS\n");
			break;
		case ICMP_UNREACH_FILTER_PROHIB:
			(void)printf("Route administratively prohibited\n");
			break;
		case ICMP_UNREACH_HOST_PRECEDENCE:
			(void)printf("Host Precedence Violation\n");
			break;
		case ICMP_UNREACH_PRECEDENCE_CUTOFF:
			(void)printf("Precedence Cutoff\n");
			break;
		default:
			(void)printf("Dest Unreachable, Unknown Code: %d\n",
			    icp->icmp_code);
			break;
		}
		/* Print returned IP header information */
#ifndef icmp_data
		pr_retip(&icp->icmp_ip);
#else
		pr_retip((struct ip *)icp->icmp_data);
#endif
		break;
	case ICMP_SOURCEQUENCH:
		(void)printf("Source Quench\n");
#ifndef icmp_data
		pr_retip(&icp->icmp_ip);
#else
		pr_retip((struct ip *)icp->icmp_data);
#endif
		break;
	case ICMP_REDIRECT:
		switch(icp->icmp_code) {
		case ICMP_REDIRECT_NET:
			(void)printf("Redirect Network");
			break;
		case ICMP_REDIRECT_HOST:
			(void)printf("Redirect Host");
			break;
		case ICMP_REDIRECT_TOSNET:
			(void)printf("Redirect Type of Service and Network");
			break;
		case ICMP_REDIRECT_TOSHOST:
			(void)printf("Redirect Type of Service and Host");
			break;
		default:
			(void)printf("Redirect, Unknown Code: %d", icp->icmp_code);
			break;
		}
		(void)printf("(New addr: %s)\n",
		    inet_ntoa(icp->icmp_gwaddr));
#ifndef icmp_data
		pr_retip(&icp->icmp_ip);
#else
		pr_retip((struct ip *)icp->icmp_data);
#endif
		break;
	case ICMP_ECHO:
		(void)printf("Echo Request\n");
		/* XXX ID + Seq + Data */
		break;
	case ICMP_ROUTERADVERT:
		/* RFC1256 */
		(void)printf("Router Discovery Advertisement\n");
		(void)printf("(%d entries, lifetime %d seconds)\n",
		    icp->icmp_num_addrs, ntohs(icp->icmp_lifetime));
		break;
	case ICMP_ROUTERSOLICIT:
		/* RFC1256 */
		(void)printf("Router Discovery Solicitation\n");
		break;
	case ICMP_TIMXCEED:
		switch(icp->icmp_code) {
		case ICMP_TIMXCEED_INTRANS:
			(void)printf("Time to live exceeded\n");
			break;
		case ICMP_TIMXCEED_REASS:
			(void)printf("Frag reassembly time exceeded\n");
			break;
		default:
			(void)printf("Time exceeded, Unknown Code: %d\n",
			    icp->icmp_code);
			break;
		}
#ifndef icmp_data
		pr_retip(&icp->icmp_ip);
#else
		pr_retip((struct ip *)icp->icmp_data);
#endif
		break;
	case ICMP_PARAMPROB:
		switch(icp->icmp_code) {
		case ICMP_PARAMPROB_OPTABSENT:
			(void)printf("Parameter problem, required option "
			    "absent: pointer = 0x%02x\n",
			    ntohs(icp->icmp_hun.ih_pptr));
			break;
		default:
			(void)printf("Parameter problem: pointer = 0x%02x\n",
			    ntohs(icp->icmp_hun.ih_pptr));
			break;
		}
#ifndef icmp_data
		pr_retip(&icp->icmp_ip);
#else
		pr_retip((struct ip *)icp->icmp_data);
#endif
		break;
	case ICMP_TSTAMP:
		(void)printf("Timestamp\n");
		/* XXX ID + Seq + 3 timestamps */
		break;
	case ICMP_TSTAMPREPLY:
		(void)printf("Timestamp Reply\n");
		/* XXX ID + Seq + 3 timestamps */
		break;
	case ICMP_IREQ:
		(void)printf("Information Request\n");
		/* XXX ID + Seq */
		break;
	case ICMP_IREQREPLY:
		(void)printf("Information Reply\n");
		/* XXX ID + Seq */
		break;
#ifdef ICMP_MASKREQ
	case ICMP_MASKREQ:
		(void)printf("Address Mask Request\n");
		break;
#endif
#ifdef ICMP_MASKREPLY
	case ICMP_MASKREPLY:
		(void)printf("Address Mask Reply (Mask 0x%08x)\n",
		    ntohl(icp->icmp_mask));
		break;
#endif
	default:
		(void)printf("Unknown ICMP type: %d\n", icp->icmp_type);
	}
}

/*
 * pr_iph --
 *	Print an IP header with options.
 */
void
pr_iph(struct ip *ip)
{
	int hlen;
	u_char *cp;

	hlen = ip->ip_hl << 2;
	cp = (u_char *)ip + 20;		/* point to options */

	(void)printf("Vr HL TOS  Len   ID Flg  off TTL Pro  cks      Src      Dst Data\n");
	(void)printf(" %1x  %1x  %02x %04x %04x",
	    ip->ip_v, ip->ip_hl, ip->ip_tos, ip->ip_len, ip->ip_id);
	(void)printf("   %1x %04x", ((ip->ip_off) & 0xe000) >> 13,
	    (ip->ip_off) & 0x1fff);
	(void)printf("  %02x  %02x %04x", ip->ip_ttl, ip->ip_p, ip->ip_sum);
	(void)printf(" %s ", inet_ntoa(*(struct in_addr *)&ip->ip_src.s_addr));
	(void)printf(" %s ", inet_ntoa(*(struct in_addr *)&ip->ip_dst.s_addr));
	/* dump and option bytes */
	while (hlen-- > 20) {
		(void)printf("%02x", *cp++);
	}
	(void)putchar('\n');
}

/*
 * pr_addr --
 *	Return an ascii host address as a dotted quad and optionally with
 * a hostname.
 */
char *
pr_addr(in_addr_t a)
{
	struct hostent *hp;
	struct in_addr in;
	static char buf[16+3+MAXHOSTNAMELEN];

	in.s_addr = a;
	if ((options & F_NUMERIC) ||
	    !(hp = gethostbyaddr((char *)&in.s_addr, sizeof(in.s_addr), AF_INET)))
		(void)snprintf(buf, sizeof buf, "%s", inet_ntoa(in));
	else
		(void)snprintf(buf, sizeof buf, "%s (%s)", hp->h_name,
		    inet_ntoa(in));
	return(buf);
}

/*
 * pr_retip --
 *	Dump some info on a returned (via ICMP) IP packet.
 */
void
pr_retip(struct ip *ip)
{
	int hlen;
	u_char *cp;

	pr_iph(ip);
	hlen = ip->ip_hl << 2;
	cp = (u_char *)ip + hlen;

	if (ip->ip_p == 6)
		(void)printf("TCP: from port %u, to port %u (decimal)\n",
		    (*cp * 256 + *(cp + 1)), (*(cp + 2) * 256 + *(cp + 3)));
	else if (ip->ip_p == 17)
		(void)printf("UDP: from port %u, to port %u (decimal)\n",
		    (*cp * 256 + *(cp + 1)), (*(cp + 2) * 256 + *(cp + 3)));
}

void
fill(char *bp, char *patp)
{
	int ii, jj, kk;
	int pat[16];
	char *cp;

	for (cp = patp; *cp; cp++)
		if (!isxdigit(*cp))
			errx(1, "patterns must be specified as hex digits");
	ii = sscanf(patp,
	    "%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x",
	    &pat[0], &pat[1], &pat[2], &pat[3], &pat[4], &pat[5], &pat[6],
	    &pat[7], &pat[8], &pat[9], &pat[10], &pat[11], &pat[12],
	    &pat[13], &pat[14], &pat[15]);

	if (ii > 0)
		for (kk = 0;
		    kk <= MAXPAYLOAD - (8 + sizeof(struct tvi) + ii);
		    kk += ii)
			for (jj = 0; jj < ii; ++jj)
				bp[jj + kk] = pat[jj];
	if (!(options & F_QUIET)) {
		(void)printf("PATTERN: 0x");
		for (jj = 0; jj < ii; ++jj)
			(void)printf("%02x", bp[jj] & 0xFF);
		(void)printf("\n");
	}
}

/*
 * when we get types of ICMP message with parts of the orig. datagram
 * we want to try to assure ourselves that it is from this instance
 * of ping, and not say, a refused finger connection or something
 */
int
check_icmph(struct ip *iph)
{
	struct icmp *icmph;

	/* only allow IP version 4 */
	if (iph->ip_v != 4)
		return 0;

	/* Only allow ICMP */
	if (iph->ip_p != IPPROTO_ICMP)
		return 0;

	icmph = (struct icmp *) (iph + (4 * iph->ip_hl));

	/* make sure it is in response to an ECHO request */
	if (icmph->icmp_type != 8)
		return 0;

	/* ok, make sure it has the right id on it */
	if (icmph->icmp_hun.ih_idseq.icd_id != ident)
		return 0;

	return 1;
}

void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: ping [-DdfLnqRrv] [-c count] [-I ifaddr] [-i wait]\n"
	    "\t[-l preload] [-p pattern] [-s packetsize] [-T tos] [-t ttl]\n"
	    "\t[-w maxwait] host\n");
	exit(1);
}
