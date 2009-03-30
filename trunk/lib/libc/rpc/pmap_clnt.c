/*	$OpenBSD: pmap_clnt.c,v 1.15 2005/08/08 08:05:35 espie Exp $ */
/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 * 
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 * 
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 * 
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 * 
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 * 
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

/*
 * pmap_clnt.c
 * Client interface to pmap rpc service.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 */

#include <unistd.h>
#include <errno.h>
#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <rpc/pmap_clnt.h>

static struct timeval timeout = { 5, 0 };
static struct timeval tottimeout = { 60, 0 };

/*
 * Set a mapping between program,version and port.
 * Calls the pmap service remotely to do the mapping.
 */
bool_t
pmap_set(u_long program, u_long version, u_int protocol, int iport)
{
	struct sockaddr_in myaddress;
	int sock = -1;
	CLIENT *client;
	struct pmap parms;
	bool_t rslt;
	u_short port = iport;

	if (get_myaddress(&myaddress) != 0)
		return (FALSE);
	myaddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	client = clntudp_bufcreate(&myaddress, PMAPPROG, PMAPVERS,
	    timeout, &sock, RPCSMALLMSGSIZE, RPCSMALLMSGSIZE);
	if (client == NULL)
		return (FALSE);
	parms.pm_prog = program;
	parms.pm_vers = version;
	parms.pm_prot = protocol;
	parms.pm_port = port;
	if (CLNT_CALL(client, PMAPPROC_SET, xdr_pmap, &parms, xdr_bool, &rslt,
	    tottimeout) != RPC_SUCCESS) {
		int save_errno = errno;

		clnt_perror(client, "Cannot register service");
		errno = save_errno;
		return (FALSE);
	}
	CLNT_DESTROY(client);
	if (sock != -1)
		(void)close(sock);
	return (rslt);
}

/*
 * Remove the mapping between program,version and port.
 * Calls the pmap service remotely to do the un-mapping.
 */
bool_t
pmap_unset(u_long program, u_long version)
{
	struct sockaddr_in myaddress;
	int sock = -1;
	CLIENT *client;
	struct pmap parms;
	bool_t rslt;

	if (get_myaddress(&myaddress) != 0)
		return (FALSE);
	myaddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	client = clntudp_bufcreate(&myaddress, PMAPPROG, PMAPVERS,
	    timeout, &sock, RPCSMALLMSGSIZE, RPCSMALLMSGSIZE);
	if (client == NULL)
		return (FALSE);
	parms.pm_prog = program;
	parms.pm_vers = version;
	parms.pm_port = parms.pm_prot = 0;
	CLNT_CALL(client, PMAPPROC_UNSET, xdr_pmap, &parms, xdr_bool, &rslt,
	    tottimeout);
	CLNT_DESTROY(client);
	if (sock != -1)
		(void)close(sock);
	return (rslt);
}
