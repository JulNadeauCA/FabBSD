/*	$OpenBSD: auth_none.c,v 1.10 2006/03/31 18:28:55 deraadt Exp $ */
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
 * auth_none.c
 * Creates a client authentication handle for passing "null" 
 * credentials and verifiers to remote systems. 
 * 
 * Copyright (C) 1984, Sun Microsystems, Inc. 
 */

#include <stdlib.h>
#include <rpc/types.h>
#include <rpc/rpc.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#define MAX_MARSHEL_SIZE 20

/*
 * Authenticator operations routines
 */
static void	authnone_destroy(struct __rpc_auth *);
static void	authnone_verf(struct __rpc_auth *);
static bool_t	authnone_validate(struct __rpc_auth *, struct opaque_auth *);
static bool_t	authnone_marshal(struct __rpc_auth *, XDR *);
static bool_t	authnone_refresh(struct __rpc_auth *);

static struct auth_ops ops = {
	authnone_verf,
	authnone_marshal,
	authnone_validate,
	authnone_refresh,
	authnone_destroy
};

static struct authnone_private {
	AUTH	no_client;
	char	marshalled_client[MAX_MARSHEL_SIZE];
	u_int	mcnt;
} *authnone_private;

AUTH *
authnone_create(void)
{
	struct authnone_private *ap = authnone_private;
	XDR xdr_stream;
	XDR *xdrs;

	if (ap == NULL) {
		ap = (struct authnone_private *)calloc(1, sizeof (*ap));
		if (ap == NULL)
			return (NULL);
		authnone_private = ap;
	}
	if (!ap->mcnt) {
		ap->no_client.ah_cred = ap->no_client.ah_verf = _null_auth;
		ap->no_client.ah_ops = &ops;
		xdrs = &xdr_stream;
		xdrmem_create(xdrs, ap->marshalled_client, (u_int)MAX_MARSHEL_SIZE,
		    XDR_ENCODE);
		(void)xdr_opaque_auth(xdrs, &ap->no_client.ah_cred);
		(void)xdr_opaque_auth(xdrs, &ap->no_client.ah_verf);
		ap->mcnt = XDR_GETPOS(xdrs);
		XDR_DESTROY(xdrs);
	}
	return (&ap->no_client);
}

/*ARGSUSED*/
static bool_t
authnone_marshal(AUTH *client, XDR *xdrs)
{
	struct authnone_private *ap = authnone_private;

	if (ap == NULL)
		return (0);
	return ((*xdrs->x_ops->x_putbytes)(xdrs,
	    ap->marshalled_client, ap->mcnt));
}

/*ARGSUSED*/
static void 
authnone_verf(struct __rpc_auth *none)
{
}

/*ARGSUSED*/
static bool_t
authnone_validate(struct __rpc_auth *none, struct opaque_auth *noauth)
{

	return (TRUE);
}

/*ARGSUSED*/
static bool_t
authnone_refresh(struct __rpc_auth *none)
{

	return (FALSE);
}

/*ARGSUSED*/
static void
authnone_destroy(struct __rpc_auth *none)
{
}
