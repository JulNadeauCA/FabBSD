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
 *	from: @(#)amq_svc.c	8.1 (Berkeley) 6/6/93
 *	$Id: amq_svc.c,v 1.7 2003/07/18 22:58:56 david Exp $
 *
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>

#include "am.h"
#include "amq.h"
extern bool_t xdr_amq_mount_info_qelem();

void
amq_program_1(struct svc_req *rqstp, SVCXPRT *transp)
{
	union {
		amq_string amqproc_mnttree_1_arg;
		amq_string amqproc_umnt_1_arg;
		amq_setopt amqproc_setopt_1_arg;
		amq_string amqproc_mount_1_arg;
	} argument;
	char *result;
	bool_t (*xdr_argument)(), (*xdr_result)();
	char *(*local)();
	extern SVCXPRT *lamqp;

	if (transp != lamqp) {
		struct sockaddr_in *fromsin = svc_getcaller(transp);

		syslog(LOG_WARNING,
		    "non-local amq attempt (might be from %s)",
		    inet_ntoa(fromsin->sin_addr));
		svcerr_noproc(transp);
		return;
	}

	switch (rqstp->rq_proc) {
	case AMQPROC_NULL:
		xdr_argument = xdr_void;
		xdr_result = xdr_void;
		local = (char *(*)()) amqproc_null_1;
		break;

	case AMQPROC_MNTTREE:
		xdr_argument = xdr_amq_string;
		xdr_result = xdr_amq_mount_tree_p;
		local = (char *(*)()) amqproc_mnttree_1;
		break;

	case AMQPROC_UMNT:
		xdr_argument = xdr_amq_string;
		xdr_result = xdr_void;
		local = (char *(*)()) amqproc_umnt_1;
		break;

	case AMQPROC_STATS:
		xdr_argument = xdr_void;
		xdr_result = xdr_amq_mount_stats;
		local = (char *(*)()) amqproc_stats_1;
		break;

	case AMQPROC_EXPORT:
		xdr_argument = xdr_void;
		xdr_result = xdr_amq_mount_tree_list;
		local = (char *(*)()) amqproc_export_1;
		break;

	case AMQPROC_SETOPT:
		xdr_argument = xdr_amq_setopt;
		xdr_result = xdr_int;
		local = (char *(*)()) amqproc_setopt_1;
		break;

	case AMQPROC_GETMNTFS:
		xdr_argument = xdr_void;
		xdr_result = xdr_amq_mount_info_qelem;
		local = (char *(*)()) amqproc_getmntfs_1;
		break;

	case AMQPROC_MOUNT:
		xdr_argument = xdr_amq_string;
		xdr_result = xdr_int;
		local = (char *(*)()) amqproc_mount_1;
		break;

	case AMQPROC_GETVERS:
		xdr_argument = xdr_void;
		xdr_result = xdr_amq_string;
		local = (char *(*)()) amqproc_getvers_1;
		break;

	default:
		svcerr_noproc(transp);
		return;
	}
	bzero((char *)&argument, sizeof(argument));
	if (!svc_getargs(transp, xdr_argument, (char *)&argument)) {
		svcerr_decode(transp);
		return;
	}
	result = (*local)(&argument, rqstp);
	if (result != NULL && !svc_sendreply(transp, xdr_result, result)) {
		svcerr_systemerr(transp);
	}
	if (!svc_freeargs(transp, xdr_argument, (char *)&argument)) {
		plog(XLOG_FATAL, "unable to free rpc arguments in amqprog_1");
		going_down(1);
	}
}
