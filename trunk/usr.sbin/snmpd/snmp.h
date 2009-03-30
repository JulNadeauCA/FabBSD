/*	$OpenBSD: snmp.h,v 1.7 2008/02/07 11:33:26 reyk Exp $	*/

/*
 * Copyright (c) 2007, 2008 Reyk Floeter <reyk@vantronix.net>
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

#ifndef SNMP_HEADER
#define SNMP_HEADER

/*
 * SNMP IMSG interface
 */

#define SNMP_MAX_OID_LEN	128	/* max size of the OID _string_ */
#define SNMP_SOCKET		"/var/run/snmpd.sock"

enum snmp_type {
	SNMP_IPADDR		= 0,
	SNMP_COUNTER32		= 1,
	SNMP_GAUGE32		= 2,
	SNMP_UNSIGNED32		= 2,
	SNMP_TIMETICKS		= 3,
	SNMP_OPAQUE		= 4,
	SNMP_NSAPADDR		= 5,
	SNMP_COUNTER64		= 6,
	SNMP_UINTEGER32		= 7,

	SNMP_INTEGER32		= 100,
	SNMP_BITSTRING		= 101,
	SNMP_OCTETSTRING	= 102,
	SNMP_NULL		= 103,
	SNMP_OBJECT		= 104
};

enum snmp_imsg_ctl {
	IMSG_SNMP_TRAP		= 1000,	/* something that works everywhere */
	IMSG_SNMP_ELEMENT,
	IMSG_SNMP_END,
	IMSG_SNMP_LOCK			/* enable restricted mode */
};

struct snmp_imsg_hdr {
	u_int16_t	 imsg_type;
	u_int16_t	 imsg_len;
	u_int32_t	 imsg_peerid;
	pid_t		 imsg_pid;
};

struct snmp_imsg {
	char		 snmp_oid[SNMP_MAX_OID_LEN];
	u_int8_t	 snmp_type;
	u_int16_t	 snmp_len;
};

/*
 * SNMP BER types
 */

enum snmp_version {
	SNMP_V1			= 0,
	SNMP_V2			= 1,	/* SNMPv2c */
	SNMP_V3			= 3
};

enum snmp_context {
	SNMP_C_GETREQ		= 0,
	SNMP_C_GETNEXTREQ	= 1,
	SNMP_C_GETRESP		= 2,
	SNMP_C_SETREQ		= 3,
	SNMP_C_TRAP		= 4,

	/* SNMPv2 */
	SNMP_C_GETBULKREQ	= 5,
	SNMP_C_INFORMREQ	= 6,
	SNMP_C_TRAPV2		= 7,
	SNMP_C_REPORT		= 8
};

enum snmp_application {
	SNMP_T_IPADDR		= 0,
	SNMP_T_COUNTER32	= 1,
	SNMP_T_GAUGE32		= 2,
	SNMP_T_UNSIGNED32	= 2,
	SNMP_T_TIMETICKS	= 3,
	SNMP_T_OPAQUE		= 4,
	SNMP_T_NSAPADDR		= 5,
	SNMP_T_COUNTER64	= 6,
	SNMP_T_UINTEGER32	= 7
};

enum snmp_generic_trap {
	SNMP_TRAP_COLDSTART	= 0,
	SNMP_TRAP_WARMSTART	= 1,
	SNMP_TRAP_LINKDOWN	= 2,
	SNMP_TRAP_LINKUP	= 3,
	SNMP_TRAP_AUTHFAILURE	= 4,
	SNMP_TRAP_EGPNEIGHLOSS	= 5,
	SNMP_TRAP_ENTERPRISE	= 6
};

enum snmp_error {
	SNMP_ERROR_NONE		= 0,
	SNMP_ERROR_TOOBIG	= 1,
	SNMP_ERROR_NOSUCHNAME	= 2,
	SNMP_ERROR_BADVALUE	= 3,
	SNMP_ERROR_READONLY	= 4,
	SNMP_ERROR_GENERR	= 5,

	/* SNMPv2 */
	SNMP_ERROR_NOACCESS	= 6,
	SNMP_ERROR_WRONGTYPE	= 7,
	SNMP_ERROR_WRONGLENGTH	= 8,
	SNMP_ERROR_WRONGENC	= 9,
	SNMP_ERROR_WRONGVALUE	= 10,
	SNMP_ERROR_NOCREATION	= 11,
	SNMP_ERROR_INCONVALUE	= 12,
	SNMP_ERROR_RESUNAVAIL	= 13, /* EGAIN */
	SNMP_ERROR_COMMITFAILED	= 14,
	SNMP_ERROR_UNDOFAILED	= 15,
	SNMP_ERROR_AUTHERROR	= 16,
	SNMP_ERROR_NOTWRITABLE	= 17,
	SNMP_ERROR_INCONNAME	= 18
};

#endif /* SNMP_HEADER */
