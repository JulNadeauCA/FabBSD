/*	$FabBSD$	*/
/*
 * Copyright (c) 1990, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from the Stanford/CMU enet packet filter,
 * (net/enet.c) distributed as part of 4.3BSD, and code contributed
 * to Berkeley by Steven McCanne and Van Jacobson both of Lawrence
 * Berkeley Laboratory.
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
 *	@(#)bpf.h	8.1 (Berkeley) 6/10/93
 */

#ifndef _NET_DATALINKTYPES_H_
#define _NET_DATALINKTYPES_H_

/*
 * Data-link level type codes.
 */
#define DLT_NULL		0	/* no link-layer encapsulation */
#define DLT_EN10MB		1	/* Ethernet (10Mb) */
#define DLT_EN3MB		2	/* Experimental Ethernet (3Mb) */
#define DLT_AX25		3	/* Amateur Radio AX.25 */
#define DLT_PRONET		4	/* Proteon ProNET Token Ring */
#define DLT_CHAOS		5	/* Chaos */
#define DLT_IEEE802		6	/* IEEE 802 Networks */
#define DLT_ARCNET		7	/* ARCNET */
#define DLT_SLIP		8	/* Serial Line IP */
#define DLT_PPP			9	/* Point-to-point Protocol */
#define DLT_FDDI		10	/* FDDI */
#define DLT_ATM_RFC1483		11	/* LLC/SNAP encapsulated atm */
#define DLT_LOOP		12	/* loopback type (af header) */
#define DLT_ENC			13	/* IPSEC enc type (af header, spi, flags) */
#define DLT_RAW			14	/* raw IP */
#define DLT_SLIP_BSDOS		15	/* BSD/OS Serial Line IP */
#define DLT_PPP_BSDOS		16	/* BSD/OS Point-to-point Protocol */
#define DLT_OLD_PFLOG		17	/* Packet filter logging, old (XXX remove?) */
#define DLT_PFSYNC		18	/* Packet filter state syncing */
#define DLT_PPP_ETHER		51	/* PPP over Ethernet; session only w/o ether header */
#define DLT_IEEE802_11		105	/* IEEE 802.11 wireless */
#define DLT_PFLOG		117	/* Packet filter logging, by pcap people */
#define DLT_IEEE802_11_RADIO	127	/* IEEE 802.11 plus WLAN header */
#define DLT_MPLS		128	/* MPLS Provider Edge header */

#endif /* _NET_DATALINKTYPES_H_ */
