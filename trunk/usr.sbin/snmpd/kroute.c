/*	$OpenBSD: kroute.c,v 1.6 2008/01/16 09:51:15 reyk Exp $	*/

/*
 * Copyright (c) 2007, 2008 Reyk Floeter <reyk@vantronix.net>
 * Copyright (c) 2004 Esben Norby <norby@openbsd.org>
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/tree.h>
#include <sys/uio.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <event.h>

#include "snmpd.h"

struct {
	u_int32_t		 ks_rtseq;
	pid_t			 ks_pid;
	int			 ks_fd;
	int			 ks_ifd;
	struct event		 ks_ev;
	u_short			 ks_nkif;
	u_long			 ks_iflastchange;
} kr_state;

struct kroute_node {
	RB_ENTRY(kroute_node)	 entry;
	struct kroute		 r;
	struct kroute_node	*next;
};

struct kif_node {
	RB_ENTRY(kif_node)	 entry;
	TAILQ_HEAD(, kif_addr)	 addrs;
	struct kif		 k;
};

int			 kroute_compare(struct kroute_node *, struct kroute_node *);
struct kroute_node	*kroute_find(in_addr_t, u_int8_t);
struct kroute_node	*kroute_match(in_addr_t);
struct kroute_node	*kroute_matchgw(struct kroute_node *, struct in_addr);
int			 kroute_insert(struct kroute_node *);
int			 kroute_remove(struct kroute_node *);
void			 kroute_clear(void);

int			 kif_init(void);
int			 kif_compare(struct kif_node *, struct kif_node *);
struct kif_node		*kif_find(u_short);
struct kif_node		*kif_insert(u_short);
int			 kif_remove(struct kif_node *);
void			 kif_clear(void);
struct kif		*kif_update(u_short, int, struct if_data *,
			    struct sockaddr_dl *);
int			 kif_validate(u_short);

int			 ka_compare(struct kif_addr *, struct kif_addr *);
struct kif_addr		*ka_insert(u_short, struct kif_addr *);
struct kif_addr		*ka_find(struct in_addr *);
int			 ka_remove(struct kif_addr *);

u_int16_t		 rtlabel_name2id(const char *);
const char		*rtlabel_id2name(u_int16_t);
void			 rtlabel_unref(u_int16_t);

int			 protect_lo(void);
u_int8_t		 prefixlen_classful(in_addr_t);
u_int8_t		 mask2prefixlen(in_addr_t);
in_addr_t		 prefixlen2mask(u_int8_t);
void			 get_rtaddrs(int, struct sockaddr *, struct sockaddr **);
void			 if_change(u_short, int, struct if_data *);
void			 if_newaddr(u_short, struct sockaddr_in *, struct sockaddr_in *,
			    struct sockaddr_in *);
void			 if_deladdr(u_short, struct sockaddr_in *, struct sockaddr_in *,
			    struct sockaddr_in *);
void			 if_announce(void *);

int			 send_rtmsg(int, int, struct kroute *);
void			 dispatch_rtmsg(int, short, void *);
int			 fetchifs(u_short);
int			 fetchtable(void);

RB_HEAD(kroute_tree, kroute_node)	krt;
RB_PROTOTYPE(kroute_tree, kroute_node, entry, kroute_compare)
RB_GENERATE(kroute_tree, kroute_node, entry, kroute_compare)

RB_HEAD(kif_tree, kif_node)		kit;
RB_PROTOTYPE(kif_tree, kif_node, entry, kif_compare)
RB_GENERATE(kif_tree, kif_node, entry, kif_compare)

RB_HEAD(ka_tree, kif_addr)		kat;
RB_PROTOTYPE(ka_tree, kif_addr, node, ka_compare)
RB_GENERATE(ka_tree, kif_addr, node, ka_compare)

int
kif_init(void)
{
	RB_INIT(&kit);

	if (fetchifs(0) == -1)
		return (-1);

	return (0);
}

int
kr_init(void)
{
	int		opt = 0, rcvbuf, default_rcvbuf;
	socklen_t	optlen;

	if (kif_init() == -1)
		return (-1);

	if ((kr_state.ks_ifd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		log_warn("kr_init: ioctl socket");
		return (-1);
	}

	if ((kr_state.ks_fd = socket(AF_ROUTE, SOCK_RAW, 0)) == -1) {
		log_warn("kr_init: route socket");
		return (-1);
	}

	/* not interested in my own messages */
	if (setsockopt(kr_state.ks_fd, SOL_SOCKET, SO_USELOOPBACK,
	    &opt, sizeof(opt)) == -1)
		log_warn("kr_init: setsockopt");	/* not fatal */

	/* grow receive buffer, don't wanna miss messages */
	optlen = sizeof(default_rcvbuf);
	if (getsockopt(kr_state.ks_fd, SOL_SOCKET, SO_RCVBUF,
	    &default_rcvbuf, &optlen) == -1)
		log_warn("kr_init getsockopt SOL_SOCKET SO_RCVBUF");
	else
		for (rcvbuf = MAX_RTSOCK_BUF;
		    rcvbuf > default_rcvbuf &&
		    setsockopt(kr_state.ks_fd, SOL_SOCKET, SO_RCVBUF,
		    &rcvbuf, sizeof(rcvbuf)) == -1 && errno == ENOBUFS;
		    rcvbuf /= 2)
			;	/* nothing */

	kr_state.ks_pid = getpid();
	kr_state.ks_rtseq = 1;

	RB_INIT(&krt);

	if (fetchtable() == -1)
		return (-1);

	if (protect_lo() == -1)
		return (-1);

	event_set(&kr_state.ks_ev, kr_state.ks_fd, EV_READ | EV_PERSIST,
	    dispatch_rtmsg, NULL);
	event_add(&kr_state.ks_ev, NULL);

	return (0);
}

void
kr_shutdown(void)
{
	kroute_clear();
	kif_clear();
}

u_int
kr_ifnumber(void)
{
	return (kr_state.ks_nkif);
}

u_long
kr_iflastchange(void)
{
	return (kr_state.ks_iflastchange);
}

int
kr_updateif(u_int if_index)
{
	struct kif_node	*kn;

	if ((kn = kif_find(if_index)) != NULL)
		kif_remove(kn);

	/* Do not update the interface address list */
	return (fetchifs(if_index));
}

/* rb-tree compare */
int
kroute_compare(struct kroute_node *a, struct kroute_node *b)
{
	if (ntohl(a->r.prefix.s_addr) < ntohl(b->r.prefix.s_addr))
		return (-1);
	if (ntohl(a->r.prefix.s_addr) > ntohl(b->r.prefix.s_addr))
		return (1);
	if (a->r.prefixlen < b->r.prefixlen)
		return (-1);
	if (a->r.prefixlen > b->r.prefixlen)
		return (1);
	return (0);
}

int
kif_compare(struct kif_node *a, struct kif_node *b)
{
	return (a->k.if_index - b->k.if_index);
}

int
ka_compare(struct kif_addr *a, struct kif_addr *b)
{
	return (memcmp(&a->addr, &b->addr, sizeof(struct in_addr)));
}

/* tree management */
struct kroute_node *
kroute_find(in_addr_t prefix, u_int8_t prefixlen)
{
	struct kroute_node	s;

	s.r.prefix.s_addr = prefix;
	s.r.prefixlen = prefixlen;

	return (RB_FIND(kroute_tree, &krt, &s));
}

struct kroute_node *
kroute_matchgw(struct kroute_node *kr, struct in_addr nh)
{
	in_addr_t	nexthop;

	nexthop = nh.s_addr;

	while (kr) {
		if (kr->r.nexthop.s_addr == nexthop)
			return (kr);
		kr = kr->next;
	}

	return (NULL);
}

int
kroute_insert(struct kroute_node *kr)
{
	struct kroute_node	*krm;

	if ((krm = RB_INSERT(kroute_tree, &krt, kr)) != NULL) {
		/*
		 * Multipath route, add at end of list and clone the
		 * ospfd inserted flag.
		 */
		kr->r.flags |= krm->r.flags & F_OSPFD_INSERTED;
		while (krm->next != NULL)
			krm = krm->next;
		krm->next = kr;
		kr->next = NULL; /* to be sure */
	} else
		krm = kr;

	if (!(kr->r.flags & F_KERNEL)) {
		/* don't validate or redistribute ospf route */
		kr->r.flags &= ~F_DOWN;
		return (0);
	}

	if (kif_validate(kr->r.if_index))
		kr->r.flags &= ~F_DOWN;
	else
		kr->r.flags |= F_DOWN;

	return (0);
}

int
kroute_remove(struct kroute_node *kr)
{
	struct kroute_node	*krm;

	if ((krm = RB_FIND(kroute_tree, &krt, kr)) == NULL) {
		log_warnx("kroute_remove failed to find %s/%u",
		    inet_ntoa(kr->r.prefix), kr->r.prefixlen);
		return (-1);
	}

	if (krm == kr) {
		/* head element */
		if (RB_REMOVE(kroute_tree, &krt, kr) == NULL) {
			log_warnx("kroute_remove failed for %s/%u",
			    inet_ntoa(kr->r.prefix), kr->r.prefixlen);
			return (-1);
		}
		if (kr->next != NULL) {
			if (RB_INSERT(kroute_tree, &krt, kr->next) != NULL) {
				log_warnx("kroute_remove failed to add %s/%u",
				    inet_ntoa(kr->r.prefix), kr->r.prefixlen);
				return (-1);
			}
		}
	} else {
		/* somewhere in the list */
		while (krm->next != kr && krm->next != NULL)
			krm = krm->next;
		if (krm->next == NULL) {
			log_warnx("kroute_remove multipath list corrupted "
			    "for %s/%u", inet_ntoa(kr->r.prefix),
			    kr->r.prefixlen);
			return (-1);
		}
		krm->next = kr->next;
	}

	rtlabel_unref(kr->r.rtlabel);

	free(kr);
	return (0);
}

void
kroute_clear(void)
{
	struct kroute_node	*kr;

	while ((kr = RB_MIN(kroute_tree, &krt)) != NULL)
		kroute_remove(kr);
}

struct kif_node *
kif_find(u_short if_index)
{
	struct kif_node	s;

	bzero(&s, sizeof(s));
	s.k.if_index = if_index;

	return (RB_FIND(kif_tree, &kit, &s));
}

struct kif *
kr_getif(u_short if_index)
{
	struct kif_node	*kn;

	if (if_index == 0)
		kn = RB_MIN(kif_tree, &kit);
	else
		kn = kif_find(if_index);
	if (kn == NULL)
		return (NULL);

	return (&kn->k);
}

struct kif *
kr_getnextif(u_short if_index)
{
	struct kif_node	*kn;

	if (if_index == 0) {
		kn = RB_MIN(kif_tree, &kit);
		return (&kn->k);
	}

	if ((kn = kif_find(if_index)) == NULL)
		return (NULL);
	kn = RB_NEXT(kif_tree, &kit, kn);
	if (kn == NULL)
		return (NULL);

	return (&kn->k);
}

struct kif_node *
kif_insert(u_short if_index)
{
	struct kif_node	*kif;

	if ((kif = calloc(1, sizeof(struct kif_node))) == NULL)
		return (NULL);

	kif->k.if_index = if_index;
	TAILQ_INIT(&kif->addrs);

	if (RB_INSERT(kif_tree, &kit, kif) != NULL)
		fatalx("kif_insert: RB_INSERT");

	kr_state.ks_nkif++;
	kr_state.ks_iflastchange = smi_getticks();

	return (kif);
}

int
kif_remove(struct kif_node *kif)
{
	struct kif_addr	*ka;

	if (RB_REMOVE(kif_tree, &kit, kif) == NULL) {
		log_warnx("RB_REMOVE(kif_tree, &kit, kif)");
		return (-1);
	}

	while ((ka = TAILQ_FIRST(&kif->addrs)) != NULL) {
		TAILQ_REMOVE(&kif->addrs, ka, entry);
		ka_remove(ka);
	}
	free(kif);

	kr_state.ks_nkif--;
	kr_state.ks_iflastchange = smi_getticks();

	return (0);
}

void
kif_clear(void)
{
	struct kif_node	*kif;

	while ((kif = RB_MIN(kif_tree, &kit)) != NULL)
		kif_remove(kif);
	kr_state.ks_nkif = 0;
	kr_state.ks_iflastchange = smi_getticks();
}

struct kif *
kif_update(u_short if_index, int flags, struct if_data *ifd,
    struct sockaddr_dl *sdl)
{
	struct kif_node		*kif;
	struct ether_addr	*ea;
	struct ifreq		 ifr;

	if ((kif = kif_find(if_index)) == NULL)
		if ((kif = kif_insert(if_index)) == NULL)
			return (NULL);

	kif->k.if_flags = flags;
	bcopy(ifd, &kif->k.if_data, sizeof(struct if_data));
	kif->k.if_ticks = smi_getticks();

	if (sdl && sdl->sdl_family == AF_LINK) {
		if (sdl->sdl_nlen >= sizeof(kif->k.if_name))
			memcpy(kif->k.if_name, sdl->sdl_data,
			    sizeof(kif->k.if_name) - 1);
		else if (sdl->sdl_nlen > 0)
			memcpy(kif->k.if_name, sdl->sdl_data,
			    sdl->sdl_nlen);
		/* string already terminated via calloc() */

		if ((ea = (struct ether_addr *)LLADDR(sdl)) != NULL)
			bcopy(&ea->ether_addr_octet, kif->k.if_lladdr,
			    ETHER_ADDR_LEN);
	}

	bzero(&ifr, sizeof(ifr));
	strlcpy(ifr.ifr_name, kif->k.if_name, sizeof(ifr.ifr_name));
	ifr.ifr_data = (caddr_t)&kif->k.if_descr;
	if (ioctl(kr_state.ks_ifd, SIOCGIFDESCR, &ifr) == -1)
		bzero(&kif->k.if_descr, sizeof(kif->k.if_descr));

	return (&kif->k);
}

int
kif_validate(u_short if_index)
{
	struct kif_node		*kif;

	if ((kif = kif_find(if_index)) == NULL) {
		log_warnx("interface with index %u not found", if_index);
		return (1);
	}

	return (kif->k.if_nhreachable);
}

struct kroute_node *
kroute_match(in_addr_t key)
{
	int			 i;
	struct kroute_node	*kr;

	/* we will never match the default route */
	for (i = 32; i > 0; i--)
		if ((kr = kroute_find(key & prefixlen2mask(i), i)) != NULL)
			return (kr);

	/* if we don't have a match yet, try to find a default route */
	if ((kr = kroute_find(0, 0)) != NULL)
			return (kr);

	return (NULL);
}

struct kif_addr *
ka_insert(u_short if_index, struct kif_addr *ka)
{
	if (ka->addr.s_addr == INADDR_ANY)
		return (ka);
	ka->if_index = if_index;
	return (RB_INSERT(ka_tree, &kat, ka));
}

struct kif_addr	*
ka_find(struct in_addr *in)
{
	struct kif_addr		ka;

	ka.addr.s_addr = in->s_addr;
	return (RB_FIND(ka_tree, &kat, &ka));
}

int
ka_remove(struct kif_addr *ka)
{
	RB_REMOVE(ka_tree, &kat, ka);
	free(ka);
	return (0);
}

struct kif_addr *
kr_getaddr(struct in_addr *in)
{
	struct kif_addr	*ka;

	if (in == NULL)
		ka = RB_MIN(ka_tree, &kat);
	else
		ka = ka_find(in);
	if (ka == NULL)
		return (NULL);

	return (ka);
}

struct kif_addr *
kr_getnextaddr(struct in_addr *in)
{
	struct kif_addr	*ka;

	if (in == NULL)
		return (RB_MIN(ka_tree, &kat));

	if ((ka = ka_find(in)) == NULL)
		return (NULL);
	ka = RB_NEXT(ka_tree, &kat, ka);
	if (ka == NULL)
		return (NULL);

	return (ka);
}

/* misc */
int
protect_lo(void)
{
	struct kroute_node	*kr;

	/* special protection for 127/8 */
	if ((kr = calloc(1, sizeof(struct kroute_node))) == NULL) {
		log_warn("protect_lo");
		return (-1);
	}
	kr->r.prefix.s_addr = htonl(INADDR_LOOPBACK);
	kr->r.prefixlen = 8;
	kr->r.flags = F_KERNEL|F_CONNECTED;

	if (RB_INSERT(kroute_tree, &krt, kr) != NULL)
		free(kr);	/* kernel route already there, no problem */

	return (0);
}

u_int8_t
prefixlen_classful(in_addr_t ina)
{
	/* it hurt to write this. */

	if (ina >= 0xf0000000U)		/* class E */
		return (32);
	else if (ina >= 0xe0000000U)	/* class D */
		return (4);
	else if (ina >= 0xc0000000U)	/* class C */
		return (24);
	else if (ina >= 0x80000000U)	/* class B */
		return (16);
	else				/* class A */
		return (8);
}

u_int8_t
mask2prefixlen(in_addr_t ina)
{
	if (ina == 0)
		return (0);
	else
		return (33 - ffs(ntohl(ina)));
}

in_addr_t
prefixlen2mask(u_int8_t prefixlen)
{
	if (prefixlen == 0)
		return (0);

	return (htonl(0xffffffff << (32 - prefixlen)));
}

#define	ROUNDUP(a, size)	\
    (((a) & ((size) - 1)) ? (1 + ((a) | ((size) - 1))) : (a))

void
get_rtaddrs(int addrs, struct sockaddr *sa, struct sockaddr **rti_info)
{
	int	i;

	for (i = 0; i < RTAX_MAX; i++) {
		if (addrs & (1 << i)) {
			rti_info[i] = sa;
			sa = (struct sockaddr *)((char *)(sa) +
			    ROUNDUP(sa->sa_len, sizeof(long)));
		} else
			rti_info[i] = NULL;

	}
}

void
if_change(u_short if_index, int flags, struct if_data *ifd)
{
	struct kroute_node	*kr, *tkr;
	struct kif		*kif;
	u_int8_t		 reachable;

	if ((kif = kif_update(if_index, flags, ifd, NULL)) == NULL) {
		log_warn("if_change:  kif_update(%u)", if_index);
		return;
	}

	reachable = (kif->if_flags & IFF_UP) &&
	    (LINK_STATE_IS_UP(kif->if_link_state) ||
	    (kif->if_link_state == LINK_STATE_UNKNOWN &&
	    kif->if_type != IFT_CARP));

	if (reachable == kif->if_nhreachable)
		return;		/* nothing changed wrt nexthop validity */

	kif->if_nhreachable = reachable;

#ifdef notyet
	/* notify ospfe about interface link state */
	main_imsg_compose_ospfe(IMSG_IFINFO, 0, kif, sizeof(struct kif));
#endif

	/* update redistribute list */
	RB_FOREACH(kr, kroute_tree, &krt) {
		for (tkr = kr; tkr != NULL; tkr = tkr->next) {
			if (tkr->r.if_index == if_index) {
				if (reachable)
					tkr->r.flags &= ~F_DOWN;
				else
					tkr->r.flags |= F_DOWN;
			}
		}
	}
}

void
if_newaddr(u_short if_index, struct sockaddr_in *ifa, struct sockaddr_in *mask,
    struct sockaddr_in *brd)
{
	struct kif_node *kif;
	struct kif_addr *ka;

	if (ifa == NULL || ifa->sin_family != AF_INET)
		return;
	if ((kif = kif_find(if_index)) == NULL) {
		log_warnx("if_newaddr: corresponding if %i not found",
		    if_index);
		return;
	}
	if (ka_find(&ifa->sin_addr) != NULL)
		return;
	if ((ka = calloc(1, sizeof(struct kif_addr))) == NULL)
		fatal("if_newaddr");
	ka->addr = ifa->sin_addr;
	if (mask)
		ka->mask = mask->sin_addr;
	else
		ka->mask.s_addr = INADDR_NONE;
	if (brd)
		ka->dstbrd = brd->sin_addr;
	else
		ka->dstbrd.s_addr = INADDR_NONE;

	TAILQ_INSERT_TAIL(&kif->addrs, ka, entry);
	ka_insert(if_index, ka);
}

void
if_deladdr(u_short if_index, struct sockaddr_in *ifa, struct sockaddr_in *mask,
    struct sockaddr_in *brd)
{
	struct kif_node *kif;
	struct kif_addr *ka;

	if (ifa == NULL || ifa->sin_family != AF_INET)
		return;
	if ((kif = kif_find(if_index)) == NULL) {
		log_warnx("if_newaddr: corresponding if %i not found",
		    if_index);
		return;
	}
	if ((ka = ka_find(&ifa->sin_addr)) == NULL)
		return;

	TAILQ_REMOVE(&kif->addrs, ka, entry);
	ka_remove(ka);
}

void
if_announce(void *msg)
{
	struct if_announcemsghdr	*ifan;
	struct kif_node			*kif;

	ifan = msg;

	switch (ifan->ifan_what) {
	case IFAN_ARRIVAL:
		kif = kif_insert(ifan->ifan_index);
		strlcpy(kif->k.if_name, ifan->ifan_name,
		    sizeof(kif->k.if_name));
		break;
	case IFAN_DEPARTURE:
		kif = kif_find(ifan->ifan_index);
		kif_remove(kif);
		break;
	}
}

int
fetchtable(void)
{
	size_t			 len;
	int			 mib[7];
	char			*buf, *next, *lim;
	struct rt_msghdr	*rtm;
	struct sockaddr		*sa, *rti_info[RTAX_MAX];
	struct sockaddr_in	*sa_in;
	struct sockaddr_rtlabel	*label;
	struct kroute_node	*kr;

	mib[0] = CTL_NET;
	mib[1] = AF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET;
	mib[4] = NET_RT_DUMP;
	mib[5] = 0;
	mib[6] = 0;	/* rtableid */

	if (sysctl(mib, 7, NULL, &len, NULL, 0) == -1) {
		log_warn("sysctl");
		return (-1);
	}
	if ((buf = malloc(len)) == NULL) {
		log_warn("fetchtable");
		return (-1);
	}
	if (sysctl(mib, 7, buf, &len, NULL, 0) == -1) {
		log_warn("sysctl");
		free(buf);
		return (-1);
	}

	lim = buf + len;
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		sa = (struct sockaddr *)(rtm + 1);
		get_rtaddrs(rtm->rtm_addrs, sa, rti_info);

		if ((sa = rti_info[RTAX_DST]) == NULL)
			continue;

		if (rtm->rtm_flags & RTF_LLINFO)	/* arp cache */
			continue;

		if ((kr = calloc(1, sizeof(struct kroute_node))) == NULL) {
			log_warn("fetchtable");
			free(buf);
			return (-1);
		}

		kr->r.flags = F_KERNEL;

		switch (sa->sa_family) {
		case AF_INET:
			kr->r.prefix.s_addr =
			    ((struct sockaddr_in *)sa)->sin_addr.s_addr;
			sa_in = (struct sockaddr_in *)rti_info[RTAX_NETMASK];
			if (rtm->rtm_flags & RTF_STATIC)
				kr->r.flags |= F_STATIC;
			if (rtm->rtm_flags & RTF_DYNAMIC)
				kr->r.flags |= F_DYNAMIC;
			if (rtm->rtm_flags & RTF_PROTO1)
				kr->r.flags |= F_BGPD_INSERTED;
			if (sa_in != NULL) {
				if (sa_in->sin_len == 0)
					break;
				kr->r.prefixlen =
				    mask2prefixlen(sa_in->sin_addr.s_addr);
			} else if (rtm->rtm_flags & RTF_HOST)
				kr->r.prefixlen = 32;
			else
				kr->r.prefixlen =
				    prefixlen_classful(kr->r.prefix.s_addr);
			break;
		default:
			free(kr);
			continue;
		}

		kr->r.if_index = rtm->rtm_index;
		if ((sa = rti_info[RTAX_GATEWAY]) != NULL)
			switch (sa->sa_family) {
			case AF_INET:
				kr->r.nexthop.s_addr =
				    ((struct sockaddr_in *)sa)->sin_addr.s_addr;
				break;
			case AF_LINK:
				kr->r.flags |= F_CONNECTED;
				break;
			}

		if ((label = (struct sockaddr_rtlabel *)
		    rti_info[RTAX_LABEL]) != NULL) {
			kr->r.rtlabel =
			    rtlabel_name2id(label->sr_label);
		}
		kroute_insert(kr);
	}

	free(buf);
	return (0);
}

int
fetchifs(u_short if_index)
{
	size_t			 len;
	int			 mib[6];
	char			*buf, *next, *lim;
	struct rt_msghdr	*rtm;
	struct if_msghdr	 ifm;
	struct ifa_msghdr	*ifam;
	struct kif		*kif = NULL;
	struct sockaddr		*sa, *rti_info[RTAX_MAX];

	mib[0] = CTL_NET;
	mib[1] = AF_ROUTE;
	mib[2] = 0;
	mib[3] = 0;	/* wildcard address family */
	mib[4] = NET_RT_IFLIST;
	mib[5] = if_index;

	if (sysctl(mib, 6, NULL, &len, NULL, 0) == -1) {
		log_warn("sysctl");
		return (-1);
	}
	if ((buf = malloc(len)) == NULL) {
		log_warn("fetchif");
		return (-1);
	}
	if (sysctl(mib, 6, buf, &len, NULL, 0) == -1) {
		log_warn("sysctl");
		free(buf);
		return (-1);
	}

	lim = buf + len;
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		if (rtm->rtm_version != RTM_VERSION)
			continue;
		switch (rtm->rtm_type) {
		case RTM_IFINFO:
			bcopy(rtm, &ifm, sizeof ifm);
			sa = (struct sockaddr *)(next + sizeof(ifm));
			get_rtaddrs(ifm.ifm_addrs, sa, rti_info);

			if ((kif = kif_update(ifm.ifm_index,
			    ifm.ifm_flags, &ifm.ifm_data,
			    (struct sockaddr_dl *)rti_info[RTAX_IFP])) == NULL)
				fatal("fetchifs");

			kif->if_nhreachable = (kif->if_flags & IFF_UP) &&
			    (LINK_STATE_IS_UP(ifm.ifm_data.ifi_link_state) ||
			    (ifm.ifm_data.ifi_link_state ==
			    LINK_STATE_UNKNOWN &&
			    ifm.ifm_data.ifi_type != IFT_CARP));
			break;
		case RTM_NEWADDR:
			ifam = (struct ifa_msghdr *)rtm;
			if ((ifam->ifam_addrs & (RTA_NETMASK | RTA_IFA |
			    RTA_BRD)) == 0)
				break;
			sa = (struct sockaddr *)(ifam + 1);
			get_rtaddrs(ifam->ifam_addrs, sa, rti_info);

			if_newaddr(ifam->ifam_index,
			    (struct sockaddr_in *)rti_info[RTAX_IFA],
			    (struct sockaddr_in *)rti_info[RTAX_NETMASK],
			    (struct sockaddr_in *)rti_info[RTAX_BRD]);
			break;
		}
	}
	free(buf);
	return (0);
}

/* ARGSUSED */
void
dispatch_rtmsg(int fd, short event, void *arg)
{
	char			 buf[RT_BUF_SIZE];
	ssize_t			 n;
	char			*next, *lim;
	struct rt_msghdr	*rtm;
	struct if_msghdr	 ifm;
	struct ifa_msghdr	*ifam;
	struct sockaddr		*sa, *rti_info[RTAX_MAX];
	struct sockaddr_in	*sa_in;
	struct sockaddr_rtlabel	*label;
	struct kroute_node	*kr, *okr;
	struct in_addr		 prefix, nexthop;
	u_int8_t		 prefixlen;
	int			 flags, mpath;
	u_short			 if_index = 0;

	if ((n = read(fd, &buf, sizeof(buf))) == -1) {
		log_warn("dispatch_rtmsg: read error");
		return;
	}

	if (n == 0) {
		log_warnx("routing socket closed");
		return;
	}

	lim = buf + n;
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;

		prefix.s_addr = 0;
		prefixlen = 0;
		flags = F_KERNEL;
		nexthop.s_addr = 0;
		mpath = 0;

		if (rtm->rtm_type == RTM_ADD || rtm->rtm_type == RTM_CHANGE ||
		    rtm->rtm_type == RTM_DELETE) {
			sa = (struct sockaddr *)(rtm + 1);
			get_rtaddrs(rtm->rtm_addrs, sa, rti_info);

			if (rtm->rtm_tableid != 0)
				continue;

			if (rtm->rtm_pid == kr_state.ks_pid) /* caused by us */
				continue;

			if (rtm->rtm_errno)		/* failed attempts... */
				continue;

			if (rtm->rtm_flags & RTF_LLINFO)	/* arp cache */
				continue;

#ifdef RTF_MPATH
			if (rtm->rtm_flags & RTF_MPATH)
				mpath = 1;
#endif
			switch (sa->sa_family) {
			case AF_INET:
				prefix.s_addr =
				    ((struct sockaddr_in *)sa)->sin_addr.s_addr;
				sa_in = (struct sockaddr_in *)
				    rti_info[RTAX_NETMASK];
				if (sa_in != NULL) {
					if (sa_in->sin_len != 0)
						prefixlen = mask2prefixlen(
						    sa_in->sin_addr.s_addr);
				} else if (rtm->rtm_flags & RTF_HOST)
					prefixlen = 32;
				else
					prefixlen =
					    prefixlen_classful(prefix.s_addr);
				if (rtm->rtm_flags & RTF_STATIC)
					flags |= F_STATIC;
				if (rtm->rtm_flags & RTF_DYNAMIC)
					flags |= F_DYNAMIC;
				if (rtm->rtm_flags & RTF_PROTO1)
					flags |= F_BGPD_INSERTED;
				break;
			default:
				continue;
			}

			if_index = rtm->rtm_index;
			if ((sa = rti_info[RTAX_GATEWAY]) != NULL) {
				switch (sa->sa_family) {
				case AF_INET:
					nexthop.s_addr = ((struct
					    sockaddr_in *)sa)->sin_addr.s_addr;
					break;
				case AF_LINK:
					flags |= F_CONNECTED;
					break;
				}
			}
		}

		switch (rtm->rtm_type) {
		case RTM_ADD:
		case RTM_CHANGE:
			if (nexthop.s_addr == 0 && !(flags & F_CONNECTED)) {
				log_warnx("dispatch_rtmsg no nexthop for %s/%u",
				    inet_ntoa(prefix), prefixlen);
				continue;
			}

			if ((okr = kroute_find(prefix.s_addr, prefixlen)) !=
			    NULL) {
				/* just add new multipath routes */
				if (mpath && rtm->rtm_type == RTM_ADD)
					goto add;
				/* get the correct route */
				kr = okr;
				if (mpath && (kr = kroute_matchgw(okr,
				    nexthop)) == NULL) {
					log_warnx("dispatch_rtmsg mpath route"
					    " not found");
					/* add routes we missed out earlier */
					goto add;
				}

				kr->r.nexthop.s_addr = nexthop.s_addr;
				kr->r.flags = flags;
				kr->r.if_index = if_index;
				kr->r.ticks = smi_getticks();

				rtlabel_unref(kr->r.rtlabel);
				kr->r.rtlabel = 0;
				if ((label = (struct sockaddr_rtlabel *)
				    rti_info[RTAX_LABEL]) != NULL) {
					kr->r.rtlabel =
					    rtlabel_name2id(label->sr_label);
				}

				if (kif_validate(kr->r.if_index))
					kr->r.flags &= ~F_DOWN;
				else
					kr->r.flags |= F_DOWN;
			} else {
add:
				if ((kr = calloc(1,
				    sizeof(struct kroute_node))) == NULL) {
					log_warn("dispatch_rtmsg");
					return;
				}
				kr->r.prefix.s_addr = prefix.s_addr;
				kr->r.prefixlen = prefixlen;
				kr->r.nexthop.s_addr = nexthop.s_addr;
				kr->r.flags = flags;
				kr->r.if_index = if_index;
				kr->r.ticks = smi_getticks();

				if ((label = (struct sockaddr_rtlabel *)
				    rti_info[RTAX_LABEL]) != NULL) {
					kr->r.rtlabel =
					    rtlabel_name2id(label->sr_label);
				}

				kroute_insert(kr);
			}
			break;
		case RTM_DELETE:
			if ((kr = kroute_find(prefix.s_addr, prefixlen)) ==
			    NULL)
				continue;
			if (!(kr->r.flags & F_KERNEL))
				continue;
			/* get the correct route */
			okr = kr;
			if (mpath &&
			    (kr = kroute_matchgw(kr, nexthop)) == NULL) {
				log_warnx("dispatch_rtmsg mpath route"
				    " not found");
				return;
			}
#ifdef notyet
			/*
			 * last route is getting removed request the
			 * ospf route from the RDE to insert instead
			 */
			if (okr == kr && kr->next == NULL &&
			    kr->r.flags & F_OSPFD_INSERTED)
				main_imsg_compose_rde(IMSG_KROUTE_GET, 0,
				    &kr->r, sizeof(struct kroute));
#endif
			if (kroute_remove(kr) == -1)
				return;
			break;
		case RTM_IFINFO:
			memcpy(&ifm, next, sizeof(ifm));
			if_change(ifm.ifm_index, ifm.ifm_flags,
			    &ifm.ifm_data);
			break;
		case RTM_DELADDR:
			ifam = (struct ifa_msghdr *)rtm;
			if ((ifam->ifam_addrs & (RTA_NETMASK | RTA_IFA |
			    RTA_BRD)) == 0)
				break;
			sa = (struct sockaddr *)(ifam + 1);
			get_rtaddrs(ifam->ifam_addrs, sa, rti_info);

			if_deladdr(ifam->ifam_index,
			    (struct sockaddr_in *)rti_info[RTAX_IFA],
			    (struct sockaddr_in *)rti_info[RTAX_NETMASK],
			    (struct sockaddr_in *)rti_info[RTAX_BRD]);
			break;
		case RTM_NEWADDR:
			ifam = (struct ifa_msghdr *)rtm;
			if ((ifam->ifam_addrs & (RTA_NETMASK | RTA_IFA |
			    RTA_BRD)) == 0)
				break;
			sa = (struct sockaddr *)(ifam + 1);
			get_rtaddrs(ifam->ifam_addrs, sa, rti_info);

			if_newaddr(ifam->ifam_index,
			    (struct sockaddr_in *)rti_info[RTAX_IFA],
			    (struct sockaddr_in *)rti_info[RTAX_NETMASK],
			    (struct sockaddr_in *)rti_info[RTAX_BRD]);
			break;
		case RTM_IFANNOUNCE:
			if_announce(next);
			break;
		default:
			/* ignore for now */
			break;
		}
	}
}

u_int16_t
rtlabel_name2id(const char *name)
{
	return (0);
}

const char *
rtlabel_id2name(u_int16_t id)
{
	return ("");
}

void
rtlabel_unref(u_int16_t id)
{
	/* not used */
}
