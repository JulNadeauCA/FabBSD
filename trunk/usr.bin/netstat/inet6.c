/*	$OpenBSD: inet6.c,v 1.36 2007/12/19 01:47:00 deraadt Exp $	*/
/*	BSDI inet.c,v 2.3 1995/10/24 02:19:29 prb Exp	*/
/*
 * Copyright (c) 1983, 1988, 1993
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

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/sysctl.h>

#include <net/route.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <netinet/in_systm.h>
#ifndef TCP6
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#endif
#include <netinet6/ip6_var.h>
#include <netinet6/in6_var.h>
#include <netinet6/pim6_var.h>
#include <netinet6/raw_ip6.h>

#include <arpa/inet.h>
#if 0
#include "gethostbyname2.h"
#endif
#include <netdb.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "netstat.h"

struct	socket sockb;

char	*inet6name(struct in6_addr *);
void	inet6print(struct in6_addr *, int, char *);

static	char *ip6nh[] = {
	"hop by hop",
	"ICMP",
	"IGMP",
	"#3",
	"IP",
	"#5",
	"TCP",
	"#7",
	"#8",
	"#9",
	"#10",
	"#11",
	"#12",
	"#13",
	"#14",
	"#15",
	"#16",
	"UDP",
	"#18",
	"#19",
	"#20",
	"#21",
	"IDP",
	"#23",
	"#24",
	"#25",
	"#26",
	"#27",
	"#28",
	"TP",
	"#30",
	"#31",
	"#32",
	"#33",
	"#34",
	"#35",
	"#36",
	"#37",
	"#38",
	"#39",
	"#40",
	"IP6",
	"#42",
	"routing",
	"fragment",
	"#45",
	"#46",
	"#47",
	"#48",
	"#49",
	"ESP",
	"AH",
	"#52",
	"#53",
	"#54",
	"#55",
	"#56",
	"#57",
	"ICMP6",
	"no next header",
	"destination option",
	"#61",
	"#62",
	"#63",
	"#64",
	"#65",
	"#66",
	"#67",
	"#68",
	"#69",
	"#70",
	"#71",
	"#72",
	"#73",
	"#74",
	"#75",
	"#76",
	"#77",
	"#78",
	"#79",
	"ISOIP",
	"#81",
	"#82",
	"#83",
	"#84",
	"#85",
	"#86",
	"#87",
	"#88",
	"OSPF",
	"#80",
	"#91",
	"#92",
	"#93",
	"#94",
	"#95",
	"#96",
	"Ethernet",
	"#98",
	"#99",
	"#100",
	"#101",
	"#102",
	"PIM",
	"#104",
	"#105",
	"#106",
	"#107",
	"#108",
	"#109",
	"#110",
	"#111",
	"#112",
	"#113",
	"#114",
	"#115",
	"#116",
	"#117",
	"#118",
	"#119",
	"#120",
	"#121",
	"#122",
	"#123",
	"#124",
	"#125",
	"#126",
	"#127",
	"#128",
	"#129",
	"#130",
	"#131",
	"#132",
	"#133",
	"#134",
	"#135",
	"#136",
	"#137",
	"#138",
	"#139",
	"#140",
	"#141",
	"#142",
	"#143",
	"#144",
	"#145",
	"#146",
	"#147",
	"#148",
	"#149",
	"#150",
	"#151",
	"#152",
	"#153",
	"#154",
	"#155",
	"#156",
	"#157",
	"#158",
	"#159",
	"#160",
	"#161",
	"#162",
	"#163",
	"#164",
	"#165",
	"#166",
	"#167",
	"#168",
	"#169",
	"#170",
	"#171",
	"#172",
	"#173",
	"#174",
	"#175",
	"#176",
	"#177",
	"#178",
	"#179",
	"#180",
	"#181",
	"#182",
	"#183",
	"#184",
	"#185",
	"#186",
	"#187",
	"#188",
	"#189",
	"#180",
	"#191",
	"#192",
	"#193",
	"#194",
	"#195",
	"#196",
	"#197",
	"#198",
	"#199",
	"#200",
	"#201",
	"#202",
	"#203",
	"#204",
	"#205",
	"#206",
	"#207",
	"#208",
	"#209",
	"#210",
	"#211",
	"#212",
	"#213",
	"#214",
	"#215",
	"#216",
	"#217",
	"#218",
	"#219",
	"#220",
	"#221",
	"#222",
	"#223",
	"#224",
	"#225",
	"#226",
	"#227",
	"#228",
	"#229",
	"#230",
	"#231",
	"#232",
	"#233",
	"#234",
	"#235",
	"#236",
	"#237",
	"#238",
	"#239",
	"#240",
	"#241",
	"#242",
	"#243",
	"#244",
	"#245",
	"#246",
	"#247",
	"#248",
	"#249",
	"#250",
	"#251",
	"#252",
	"#253",
	"#254",
	"#255",
};

/*
 * Dump IP6 statistics structure.
 */
void
ip6_stats(char *name)
{
	struct ip6stat ip6stat;
	int first, i;
	struct protoent *ep;
	const char *n;
	int mib[] = { CTL_NET, AF_INET6, IPPROTO_IPV6, IPV6CTL_STATS };
	size_t len = sizeof(ip6stat);

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]),
	    &ip6stat, &len, NULL, 0) == -1) {
		if (errno != ENOPROTOOPT)
			warn(name);
		return;
	}

	printf("%s:\n", name);
#define	p(f, m) if (ip6stat.f || sflag <= 1) \
	printf(m, (unsigned long long)ip6stat.f, plural(ip6stat.f))
#define	p1(f, m) if (ip6stat.f || sflag <= 1) \
	printf(m, (unsigned long long)ip6stat.f)

	p(ip6s_total, "\t%llu total packet%s received\n");
	p1(ip6s_toosmall, "\t%llu with size smaller than minimum\n");
	p1(ip6s_tooshort, "\t%llu with data size < data length\n");
	p1(ip6s_badoptions, "\t%llu with bad options\n");
	p1(ip6s_badvers, "\t%llu with incorrect version number\n");
	p(ip6s_fragments, "\t%llu fragment%s received\n");
	p(ip6s_fragdropped,
	    "\t%llu fragment%s dropped (duplicates or out of space)\n");
	p(ip6s_fragtimeout, "\t%llu fragment%s dropped after timeout\n");
	p(ip6s_fragoverflow, "\t%llu fragment%s that exceeded limit\n");
	p(ip6s_reassembled, "\t%llu packet%s reassembled ok\n");
	p(ip6s_delivered, "\t%llu packet%s for this host\n");
	p(ip6s_forward, "\t%llu packet%s forwarded\n");
	p(ip6s_cantforward, "\t%llu packet%s not forwardable\n");
	p(ip6s_redirectsent, "\t%llu redirect%s sent\n");
	p(ip6s_localout, "\t%llu packet%s sent from this host\n");
	p(ip6s_rawout, "\t%llu packet%s sent with fabricated ip header\n");
	p(ip6s_odropped,
	    "\t%llu output packet%s dropped due to no bufs, etc.\n");
	p(ip6s_noroute, "\t%llu output packet%s discarded due to no route\n");
	p(ip6s_fragmented, "\t%llu output datagram%s fragmented\n");
	p(ip6s_ofragments, "\t%llu fragment%s created\n");
	p(ip6s_cantfrag, "\t%llu datagram%s that can't be fragmented\n");
	p(ip6s_badscope, "\t%llu packet%s that violated scope rules\n");
	p(ip6s_notmember, "\t%llu multicast packet%s which we don't join\n");
	for (first = 1, i = 0; i < 256; i++)
		if (ip6stat.ip6s_nxthist[i] != 0) {
			if (first) {
				printf("\tInput packet histogram:\n");
				first = 0;
			}
			n = NULL;
			if (ip6nh[i])
				n = ip6nh[i];
			else if ((ep = getprotobynumber(i)) != NULL)
				n = ep->p_name;
			if (n)
				printf("\t\t%s: %llu\n", n,
				    (unsigned long long)ip6stat.ip6s_nxthist[i]);
			else
				printf("\t\t#%d: %llu\n", i,
				    (unsigned long long)ip6stat.ip6s_nxthist[i]);
		}
	printf("\tMbuf statistics:\n");
	p(ip6s_m1, "\t\t%llu one mbuf%s\n");
	for (first = 1, i = 0; i < 32; i++) {
		char ifbuf[IFNAMSIZ];
		if (ip6stat.ip6s_m2m[i] != 0) {
			if (first) {
				printf("\t\ttwo or more mbuf:\n");
				first = 0;
			}
			printf("\t\t\t%s = %llu\n",
			    if_indextoname(i, ifbuf),
			    (unsigned long long)ip6stat.ip6s_m2m[i]);
		}
	}
	p(ip6s_mext1, "\t\t%llu one ext mbuf%s\n");
	p(ip6s_mext2m, "\t\t%llu two or more ext mbuf%s\n");
	p(ip6s_exthdrtoolong,
	    "\t%llu packet%s whose headers are not continuous\n");
	p(ip6s_nogif, "\t%llu tunneling packet%s that can't find gif\n");
	p(ip6s_toomanyhdr,
	    "\t%llu packet%s discarded due to too many headers\n");

	/* for debugging source address selection */
#define PRINT_SCOPESTAT(s,i) do {\
		switch(i) { /* XXX hardcoding in each case */\
		case 1:\
			p(s, "\t\t%llu node-local%s\n");\
			break;\
		case 2:\
			p(s, "\t\t%llu link-local%s\n");\
			break;\
		case 5:\
			p(s, "\t\t%llu site-local%s\n");\
			break;\
		case 14:\
			p(s, "\t\t%llu global%s\n");\
			break;\
		default:\
			printf("\t\t%llu addresses scope=%x\n",\
			    (unsigned long long)ip6stat.s, i);\
		}\
	} while(0);

	p(ip6s_sources_none,
	    "\t%llu failure%s of source address selection\n");
	for (first = 1, i = 0; i < 16; i++) {
		if (ip6stat.ip6s_sources_sameif[i]) {
			if (first) {
				printf("\tsource addresses on an outgoing I/F\n");
				first = 0;
			}
			PRINT_SCOPESTAT(ip6s_sources_sameif[i], i);
		}
	}
	for (first = 1, i = 0; i < 16; i++) {
		if (ip6stat.ip6s_sources_otherif[i]) {
			if (first) {
				printf("\tsource addresses on a non-outgoing I/F\n");
				first = 0;
			}
			PRINT_SCOPESTAT(ip6s_sources_otherif[i], i);
		}
	}
	for (first = 1, i = 0; i < 16; i++) {
		if (ip6stat.ip6s_sources_samescope[i]) {
			if (first) {
				printf("\tsource addresses of same scope\n");
				first = 0;
			}
			PRINT_SCOPESTAT(ip6s_sources_samescope[i], i);
		}
	}
	for (first = 1, i = 0; i < 16; i++) {
		if (ip6stat.ip6s_sources_otherscope[i]) {
			if (first) {
				printf("\tsource addresses of a different scope\n");
				first = 0;
			}
			PRINT_SCOPESTAT(ip6s_sources_otherscope[i], i);
		}
	}
	for (first = 1, i = 0; i < 16; i++) {
		if (ip6stat.ip6s_sources_deprecated[i]) {
			if (first) {
				printf("\tdeprecated source addresses\n");
				first = 0;
			}
			PRINT_SCOPESTAT(ip6s_sources_deprecated[i], i);
		}
	}

	p1(ip6s_forward_cachehit, "\t%llu forward cache hit\n");
	p1(ip6s_forward_cachemiss, "\t%llu forward cache miss\n");
#undef p
#undef p1
}

/*
 * Dump IPv6 per-interface statistics based on RFC 2465.
 */
void
ip6_ifstats(char *ifname)
{
	struct in6_ifreq ifr;
	int s;

#define	p(f, m) if (ifr.ifr_ifru.ifru_stat.f || sflag <= 1) \
	printf(m, (unsigned long long)ifr.ifr_ifru.ifru_stat.f, \
	    plural(ifr.ifr_ifru.ifru_stat.f))
#define	p_5(f, m) if (ifr.ifr_ifru.ifru_stat.f || sflag <= 1) \
	printf(m, (unsigned long long)ip6stat.f)

	if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
		perror("Warning: socket(AF_INET6)");
		return;
	}

	strlcpy(ifr.ifr_name, ifname, sizeof ifr.ifr_name);
	printf("ip6 on %s:\n", ifr.ifr_name);

	if (ioctl(s, SIOCGIFSTAT_IN6, &ifr) < 0) {
		perror("Warning: ioctl(SIOCGIFSTAT_IN6)");
		goto end;
	}

	p(ifs6_in_receive, "\t%llu total input datagram%s\n");
	p(ifs6_in_hdrerr, "\t%llu datagram%s with invalid header received\n");
	p(ifs6_in_toobig, "\t%llu datagram%s exceeded MTU received\n");
	p(ifs6_in_noroute, "\t%llu datagram%s with no route received\n");
	p(ifs6_in_addrerr, "\t%llu datagram%s with invalid dst received\n");
	p(ifs6_in_truncated, "\t%llu truncated datagram%s received\n");
	p(ifs6_in_protounknown, "\t%llu datagram%s with unknown proto received\n");
	p(ifs6_in_discard, "\t%llu input datagram%s discarded\n");
	p(ifs6_in_deliver,
	    "\t%llu datagram%s delivered to an upper layer protocol\n");
	p(ifs6_out_forward, "\t%llu datagram%s forwarded to this interface\n");
	p(ifs6_out_request,
	    "\t%llu datagram%s sent from an upper layer protocol\n");
	p(ifs6_out_discard, "\t%llu total discarded output datagram%s\n");
	p(ifs6_out_fragok, "\t%llu output datagram%s fragmented\n");
	p(ifs6_out_fragfail, "\t%llu output datagram%s failed on fragment\n");
	p(ifs6_out_fragcreat, "\t%llu output datagram%s succeeded on fragment\n");
	p(ifs6_reass_reqd, "\t%llu incoming datagram%s fragmented\n");
	p(ifs6_reass_ok, "\t%llu datagram%s reassembled\n");
	p(ifs6_reass_fail, "\t%llu datagram%s failed on reassembling\n");
	p(ifs6_in_mcast, "\t%llu multicast datagram%s received\n");
	p(ifs6_out_mcast, "\t%llu multicast datagram%s sent\n");

  end:
	close(s);

#undef p
#undef p_5
}

static	char *icmp6names[] = {
	"#0",
	"unreach",
	"packet too big",
	"time exceed",
	"parameter problem",
	"#5",
	"#6",
	"#7",
	"#8",
	"#9",
	"#10",
	"#11",
	"#12",
	"#13",
	"#14",
	"#15",
	"#16",
	"#17",
	"#18",
	"#19",
	"#20",
	"#21",
	"#22",
	"#23",
	"#24",
	"#25",
	"#26",
	"#27",
	"#28",
	"#29",
	"#30",
	"#31",
	"#32",
	"#33",
	"#34",
	"#35",
	"#36",
	"#37",
	"#38",
	"#39",
	"#40",
	"#41",
	"#42",
	"#43",
	"#44",
	"#45",
	"#46",
	"#47",
	"#48",
	"#49",
	"#50",
	"#51",
	"#52",
	"#53",
	"#54",
	"#55",
	"#56",
	"#57",
	"#58",
	"#59",
	"#60",
	"#61",
	"#62",
	"#63",
	"#64",
	"#65",
	"#66",
	"#67",
	"#68",
	"#69",
	"#70",
	"#71",
	"#72",
	"#73",
	"#74",
	"#75",
	"#76",
	"#77",
	"#78",
	"#79",
	"#80",
	"#81",
	"#82",
	"#83",
	"#84",
	"#85",
	"#86",
	"#87",
	"#88",
	"#89",
	"#80",
	"#91",
	"#92",
	"#93",
	"#94",
	"#95",
	"#96",
	"#97",
	"#98",
	"#99",
	"#100",
	"#101",
	"#102",
	"#103",
	"#104",
	"#105",
	"#106",
	"#107",
	"#108",
	"#109",
	"#110",
	"#111",
	"#112",
	"#113",
	"#114",
	"#115",
	"#116",
	"#117",
	"#118",
	"#119",
	"#120",
	"#121",
	"#122",
	"#123",
	"#124",
	"#125",
	"#126",
	"#127",
	"echo",
	"echo reply",
	"multicast listener query",
	"multicast listener report",
	"multicast listener done",
	"router solicitation",
	"router advertisement",
	"neighbor solicitation",
	"neighbor advertisement",
	"redirect",
	"router renumbering",
	"node information request",
	"node information reply",
	"#141",
	"#142",
	"#143",
	"#144",
	"#145",
	"#146",
	"#147",
	"#148",
	"#149",
	"#150",
	"#151",
	"#152",
	"#153",
	"#154",
	"#155",
	"#156",
	"#157",
	"#158",
	"#159",
	"#160",
	"#161",
	"#162",
	"#163",
	"#164",
	"#165",
	"#166",
	"#167",
	"#168",
	"#169",
	"#170",
	"#171",
	"#172",
	"#173",
	"#174",
	"#175",
	"#176",
	"#177",
	"#178",
	"#179",
	"#180",
	"#181",
	"#182",
	"#183",
	"#184",
	"#185",
	"#186",
	"#187",
	"#188",
	"#189",
	"#180",
	"#191",
	"#192",
	"#193",
	"#194",
	"#195",
	"#196",
	"#197",
	"#198",
	"#199",
	"#200",
	"#201",
	"#202",
	"#203",
	"#204",
	"#205",
	"#206",
	"#207",
	"#208",
	"#209",
	"#210",
	"#211",
	"#212",
	"#213",
	"#214",
	"#215",
	"#216",
	"#217",
	"#218",
	"#219",
	"#220",
	"#221",
	"#222",
	"#223",
	"#224",
	"#225",
	"#226",
	"#227",
	"#228",
	"#229",
	"#230",
	"#231",
	"#232",
	"#233",
	"#234",
	"#235",
	"#236",
	"#237",
	"#238",
	"#239",
	"#240",
	"#241",
	"#242",
	"#243",
	"#244",
	"#245",
	"#246",
	"#247",
	"#248",
	"#249",
	"#250",
	"#251",
	"#252",
	"#253",
	"#254",
	"#255",
};

/*
 * Dump ICMPv6 statistics.
 */
void
icmp6_stats(char *name)
{
	struct icmp6stat icmp6stat;
	int i, first;
	int mib[] = { CTL_NET, AF_INET6, IPPROTO_ICMPV6, ICMPV6CTL_STATS };
	size_t len = sizeof(icmp6stat);

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]),
	    &icmp6stat, &len, NULL, 0) == -1) {
		if (errno != ENOPROTOOPT)
			warn(name);
		return;
	}

	printf("%s:\n", name);
#define	p(f, m) if (icmp6stat.f || sflag <= 1) \
	printf(m, (unsigned long long)icmp6stat.f, plural(icmp6stat.f))
#define p_5(f, m) if (icmp6stat.f || sflag <= 1) \
	printf(m, (unsigned long long)icmp6stat.f)

	p(icp6s_error, "\t%llu call%s to icmp6_error\n");
	p(icp6s_canterror,
	    "\t%llu error%s not generated because old message was icmp6 or so\n");
	p(icp6s_toofreq,
	    "\t%llu error%s not generated because of rate limitation\n");
	for (first = 1, i = 0; i < 256; i++)
		if (icmp6stat.icp6s_outhist[i] != 0) {
			if (first) {
				printf("\tOutput packet histogram:\n");
				first = 0;
			}
			printf("\t\t%s: %llu\n", icmp6names[i],
			    (unsigned long long)icmp6stat.icp6s_outhist[i]);
		}
	p(icp6s_badcode, "\t%llu message%s with bad code fields\n");
	p(icp6s_tooshort, "\t%llu message%s < minimum length\n");
	p(icp6s_checksum, "\t%llu bad checksum%s\n");
	p(icp6s_badlen, "\t%llu message%s with bad length\n");
	for (first = 1, i = 0; i < ICMP6_MAXTYPE; i++)
		if (icmp6stat.icp6s_inhist[i] != 0) {
			if (first) {
				printf("\tInput packet histogram:\n");
				first = 0;
			}
			printf("\t\t%s: %llu\n", icmp6names[i],
			    (unsigned long long)icmp6stat.icp6s_inhist[i]);
		}
	printf("\tHistogram of error messages to be generated:\n");
	p_5(icp6s_odst_unreach_noroute, "\t\t%llu no route\n");
	p_5(icp6s_odst_unreach_admin, "\t\t%llu administratively prohibited\n");
	p_5(icp6s_odst_unreach_beyondscope, "\t\t%llu beyond scope\n");
	p_5(icp6s_odst_unreach_addr, "\t\t%llu address unreachable\n");
	p_5(icp6s_odst_unreach_noport, "\t\t%llu port unreachable\n");
	p_5(icp6s_opacket_too_big, "\t\t%llu packet too big\n");
	p_5(icp6s_otime_exceed_transit, "\t\t%llu time exceed transit\n");
	p_5(icp6s_otime_exceed_reassembly, "\t\t%llu time exceed reassembly\n");
	p_5(icp6s_oparamprob_header, "\t\t%llu erroneous header field\n");
	p_5(icp6s_oparamprob_nextheader, "\t\t%llu unrecognized next header\n");
	p_5(icp6s_oparamprob_option, "\t\t%llu unrecognized option\n");
	p_5(icp6s_oredirect, "\t\t%llu redirect\n");
	p_5(icp6s_ounknown, "\t\t%llu unknown\n");

	p(icp6s_reflect, "\t%llu message response%s generated\n");
	p(icp6s_nd_toomanyopt, "\t%llu message%s with too many ND options\n");
	p(icp6s_nd_badopt, "\t%llu message%s with bad ND options\n");
	p(icp6s_badns, "\t%llu bad neighbor solicitation message%s\n");
	p(icp6s_badna, "\t%llu bad neighbor advertisement message%s\n");
	p(icp6s_badrs, "\t%llu bad router solicitation message%s\n");
	p(icp6s_badra, "\t%llu bad router advertisement message%s\n");
	p(icp6s_badredirect, "\t%llu bad redirect message%s\n");
	p(icp6s_pmtuchg, "\t%llu path MTU change%s\n");
#undef p
#undef p_5
}

/*
 * Dump ICMPv6 per-interface statistics based on RFC 2466.
 */
void
icmp6_ifstats(char *ifname)
{
	struct in6_ifreq ifr;
	int s;

#define	p(f, m) if (ifr.ifr_ifru.ifru_icmp6stat.f || sflag <= 1) \
	printf(m, (unsigned long long)ifr.ifr_ifru.ifru_icmp6stat.f, \
	    plural(ifr.ifr_ifru.ifru_icmp6stat.f))

	if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
		perror("Warning: socket(AF_INET6)");
		return;
	}

	strlcpy(ifr.ifr_name, ifname, sizeof ifr.ifr_name);
	printf("icmp6 on %s:\n", ifr.ifr_name);

	if (ioctl(s, SIOCGIFSTAT_ICMP6, &ifr) < 0) {
		perror("Warning: ioctl(SIOCGIFSTAT_ICMP6)");
		goto end;
	}

	p(ifs6_in_msg, "\t%llu total input message%s\n");
	p(ifs6_in_error, "\t%llu total input error message%s\n");
	p(ifs6_in_dstunreach, "\t%llu input destination unreachable error%s\n");
	p(ifs6_in_adminprohib, "\t%llu input administratively prohibited error%s\n");
	p(ifs6_in_timeexceed, "\t%llu input time exceeded error%s\n");
	p(ifs6_in_paramprob, "\t%llu input parameter problem error%s\n");
	p(ifs6_in_pkttoobig, "\t%llu input packet too big error%s\n");
	p(ifs6_in_echo, "\t%llu input echo request%s\n");
	p(ifs6_in_echoreply, "\t%llu input echo reply%s\n");
	p(ifs6_in_routersolicit, "\t%llu input router solicitation%s\n");
	p(ifs6_in_routeradvert, "\t%llu input router advertisement%s\n");
	p(ifs6_in_neighborsolicit, "\t%llu input neighbor solicitation%s\n");
	p(ifs6_in_neighboradvert, "\t%llu input neighbor advertisement%s\n");
	p(ifs6_in_redirect, "\t%llu input redirect%s\n");
	p(ifs6_in_mldquery, "\t%llu input MLD query%s\n");
	p(ifs6_in_mldreport, "\t%llu input MLD report%s\n");
	p(ifs6_in_mlddone, "\t%llu input MLD done%s\n");

	p(ifs6_out_msg, "\t%llu total output message%s\n");
	p(ifs6_out_error, "\t%llu total output error message%s\n");
	p(ifs6_out_dstunreach, "\t%llu output destination unreachable error%s\n");
	p(ifs6_out_adminprohib, "\t%llu output administratively prohibited error%s\n");
	p(ifs6_out_timeexceed, "\t%llu output time exceeded error%s\n");
	p(ifs6_out_paramprob, "\t%llu output parameter problem error%s\n");
	p(ifs6_out_pkttoobig, "\t%llu output packet too big error%s\n");
	p(ifs6_out_echo, "\t%llu output echo request%s\n");
	p(ifs6_out_echoreply, "\t%llu output echo reply%s\n");
	p(ifs6_out_routersolicit, "\t%llu output router solicitation%s\n");
	p(ifs6_out_routeradvert, "\t%llu output router advertisement%s\n");
	p(ifs6_out_neighborsolicit, "\t%llu output neighbor solicitation%s\n");
	p(ifs6_out_neighboradvert, "\t%llu output neighbor advertisement%s\n");
	p(ifs6_out_redirect, "\t%llu output redirect%s\n");
	p(ifs6_out_mldquery, "\t%llu output MLD query%s\n");
	p(ifs6_out_mldreport, "\t%llu output MLD report%s\n");
	p(ifs6_out_mlddone, "\t%llu output MLD done%s\n");

  end:
	close(s);
#undef p
}

/*
 * Dump PIM statistics structure.
 */
void
pim6_stats(char *name)
{
	struct pim6stat pim6stat;
	int mib[] = { CTL_NET, AF_INET6, IPPROTO_PIM, PIM6CTL_STATS };
	size_t len = sizeof(pim6stat);

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]),
	    &pim6stat, &len, NULL, 0) == -1) {
		if (errno != ENOPROTOOPT)
			warn(name);
		return;
	}

	printf("%s:\n", name);
#define	p(f, m) if (pim6stat.f || sflag <= 1) \
	printf(m, (unsigned long long)pim6stat.f, plural(pim6stat.f))

	p(pim6s_rcv_total, "\t%llu message%s received\n");
	p(pim6s_rcv_tooshort, "\t%llu message%s received with too few bytes\n");
	p(pim6s_rcv_badsum, "\t%llu message%s received with bad checksum\n");
	p(pim6s_rcv_badversion, "\t%llu message%s received with bad version\n");
	p(pim6s_rcv_registers, "\t%llu register%s received\n");
	p(pim6s_rcv_badregisters, "\t%llu bad register%s received\n");
	p(pim6s_snd_registers, "\t%llu register%s sent\n");
#undef p
}

/*
 * Dump raw ip6 statistics structure.
 */
void
rip6_stats(char *name)
{
	struct rip6stat rip6stat;
	u_int64_t delivered;
	int mib[] = { CTL_NET, AF_INET6, IPPROTO_RAW, RIPV6CTL_STATS };
	size_t len = sizeof(rip6stat);

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]),
	    &rip6stat, &len, NULL, 0) == -1) {
		if (errno != ENOPROTOOPT)
			warn(name);
		return;
	}

	printf("%s:\n", name);

#define	p(f, m) if (rip6stat.f || sflag <= 1) \
    printf(m, (unsigned long long)rip6stat.f, plural(rip6stat.f))
	p(rip6s_ipackets, "\t%llu message%s received\n");
	p(rip6s_isum, "\t%llu checksum calculation%s on inbound\n");
	p(rip6s_badsum, "\t%llu message%s with bad checksum\n");
	p(rip6s_nosock, "\t%llu message%s dropped due to no socket\n");
	p(rip6s_nosockmcast,
	    "\t%llu multicast message%s dropped due to no socket\n");
	p(rip6s_fullsock,
	    "\t%llu message%s dropped due to full socket buffers\n");
	delivered = rip6stat.rip6s_ipackets -
		    rip6stat.rip6s_badsum -
		    rip6stat.rip6s_nosock -
		    rip6stat.rip6s_nosockmcast -
		    rip6stat.rip6s_fullsock;
	if (delivered || sflag <= 1)
		printf("\t%llu delivered\n", (unsigned long long)delivered);
	p(rip6s_opackets, "\t%llu datagram%s output\n");
#undef p
}

/*
 * Pretty print an Internet address (net address + port).
 * If the nflag was specified, use numbers instead of names.
 */

void
inet6print(struct in6_addr *in6, int port, char *proto)
{

#define GETSERVBYPORT6(port, proto, ret) do { \
	if (strcmp((proto), "tcp6") == 0) \
		(ret) = getservbyport((int)(port), "tcp"); \
	else if (strcmp((proto), "udp6") == 0) \
		(ret) = getservbyport((int)(port), "udp"); \
	else \
		(ret) = getservbyport((int)(port), (proto)); \
	} while (0)

	struct servent *sp = 0;
	char line[80], *cp;
	int width;
	int len = sizeof line;

	width = Aflag ? 12 : 16;
	if (vflag && width < strlen(inet6name(in6)))
		width = strlen(inet6name(in6));
	snprintf(line, len, "%.*s.", width, inet6name(in6));
	len -= strlen(line);
	if (len <= 0)
		goto bail;

	cp = strchr(line, '\0');
	if (!nflag && port)
		GETSERVBYPORT6(port, proto, sp);
	if (sp || port == 0)
		snprintf(cp, len, "%.8s", sp ? sp->s_name : "*");
	else
		snprintf(cp, len, "%d", ntohs((u_short)port));
	width = Aflag ? 18 : 22;
	if (vflag && width < strlen(line))
		width = strlen(line);
bail:
	printf(" %-*.*s", width, width, line);
}

/*
 * Construct an Internet address representation.
 * If the nflag has been supplied, give
 * numeric value, otherwise try for symbolic name.
 */

char *
inet6name(struct in6_addr *in6p)
{
	char *cp;
	static char line[NI_MAXHOST];
	struct hostent *hp;
	static char domain[MAXHOSTNAMELEN];
	static int first = 1;
	char hbuf[NI_MAXHOST];
	struct sockaddr_in6 sin6;
	const int niflag = NI_NUMERICHOST;

	if (first && !nflag) {
		first = 0;
		if (gethostname(domain, sizeof(domain)) == 0 &&
		    (cp = strchr(domain, '.')))
			(void) strlcpy(domain, cp + 1, sizeof domain);
		else
			domain[0] = '\0';
	}
	cp = 0;
	if (!nflag && !IN6_IS_ADDR_UNSPECIFIED(in6p)) {
		hp = gethostbyaddr((char *)in6p, sizeof(*in6p), AF_INET6);
		if (hp) {
			if ((cp = strchr(hp->h_name, '.')) &&
			    !strcmp(cp + 1, domain))
				*cp = 0;
			cp = hp->h_name;
		}
	}
	if (IN6_IS_ADDR_UNSPECIFIED(in6p))
		strlcpy(line, "*", sizeof(line));
	else if (cp)
		strlcpy(line, cp, sizeof(line));
	else {
		memset(&sin6, 0, sizeof(sin6));
		sin6.sin6_len = sizeof(sin6);
		sin6.sin6_family = AF_INET6;
		sin6.sin6_addr = *in6p;
#ifdef __KAME__
		if (IN6_IS_ADDR_LINKLOCAL(in6p) ||
		    IN6_IS_ADDR_MC_LINKLOCAL(in6p) ||
		    IN6_IS_ADDR_MC_INTFACELOCAL(in6p)) {
			sin6.sin6_scope_id =
			    ntohs(*(u_int16_t *)&in6p->s6_addr[2]);
			sin6.sin6_addr.s6_addr[2] = 0;
			sin6.sin6_addr.s6_addr[3] = 0;
		}
#endif
		if (getnameinfo((struct sockaddr *)&sin6, sin6.sin6_len,
		    hbuf, sizeof(hbuf), NULL, 0, niflag) != 0)
			strlcpy(hbuf, "?", sizeof hbuf);
		strlcpy(line, hbuf, sizeof(line));
	}
	return (line);
}

#ifdef TCP6
/*
 * Dump the contents of a TCP6 PCB.
 */
void
tcp6_dump(u_long pcbaddr)
{
	struct tcp6cb tcp6cb;
	int i;

	kread(pcbaddr, &tcp6cb, sizeof(tcp6cb));

	printf("TCP Protocol Control Block at 0x%08lx:\n\n", pcbaddr);

	printf("Timers:\n");
	for (i = 0; i < TCP6T_NTIMERS; i++)
		printf("\t%s: %u", tcp6timers[i], tcp6cb.t_timer[i]);
	printf("\n\n");

	if (tcp6cb.t_state < 0 || tcp6cb.t_state >= TCP6_NSTATES)
		printf("State: %d", tcp6cb.t_state);
	else
		printf("State: %s", tcp6states[tcp6cb.t_state]);
	printf(", flags 0x%x, in6pcb 0x%lx\n\n", tcp6cb.t_flags,
	    (u_long)tcp6cb.t_in6pcb);

	printf("rxtshift %d, rxtcur %d, dupacks %d\n", tcp6cb.t_rxtshift,
	    tcp6cb.t_rxtcur, tcp6cb.t_dupacks);
	printf("peermaxseg %u, maxseg %u, force %d\n\n", tcp6cb.t_peermaxseg,
	    tcp6cb.t_maxseg, tcp6cb.t_force);

	printf("snd_una %u, snd_nxt %u, snd_up %u\n",
	    tcp6cb.snd_una, tcp6cb.snd_nxt, tcp6cb.snd_up);
	printf("snd_wl1 %u, snd_wl2 %u, iss %u, snd_wnd %lu\n\n",
	    tcp6cb.snd_wl1, tcp6cb.snd_wl2, tcp6cb.iss, tcp6cb.snd_wnd);

	printf("rcv_wnd %lu, rcv_nxt %u, rcv_up %u, irs %u\n\n",
	    tcp6cb.rcv_wnd, tcp6cb.rcv_nxt, tcp6cb.rcv_up, tcp6cb.irs);

	printf("rcv_adv %u, snd_max %u, snd_cwnd %lu, snd_ssthresh %lu\n",
	    tcp6cb.rcv_adv, tcp6cb.snd_max, tcp6cb.snd_cwnd, tcp6cb.snd_ssthresh);

	printf("idle %d, rtt %d, rtseq %u, srtt %d, rttvar %d, rttmin %d, "
	    "max_sndwnd %lu\n\n", tcp6cb.t_idle, tcp6cb.t_rtt, tcp6cb.t_rtseq,
	    tcp6cb.t_srtt, tcp6cb.t_rttvar, tcp6cb.t_rttmin, tcp6cb.max_sndwnd);

	printf("oobflags %d, iobc %d, softerror %d\n\n", tcp6cb.t_oobflags,
	    tcp6cb.t_iobc, tcp6cb.t_softerror);

	printf("snd_scale %d, rcv_scale %d, req_r_scale %d, req_s_scale %d\n",
	    tcp6cb.snd_scale, tcp6cb.rcv_scale, tcp6cb.request_r_scale,
	    tcp6cb.requested_s_scale);
	printf("ts_recent %u, ts_regent_age %d, last_ack_sent %u\n",
	    tcp6cb.ts_recent, tcp6cb.ts_recent_age, tcp6cb.last_ack_sent);
}
#endif
