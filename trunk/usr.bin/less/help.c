/*	$OpenBSD: help.c,v 1.5 2003/04/14 14:33:57 millert Exp $	*/

/*
 * Copyright (c) 1984,1985,1989,1994,1995  Mark Nudelman
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice in the documentation and/or other materials provided with 
 *    the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR 
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN 
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/*
 * Display some help.
 * Just invoke another "less" to display the help file.
 *
 * {{ This makes this function very simple, and makes changing the
 *    help file very easy, but it may present difficulties on
 *    (non-Unix) systems which do not supply the "system()" function. }}
 */

#include  "less.h"

extern char *progname;

	public void
help(nomsg)
	int nomsg;
{
#ifdef HELPFILE
	char *helpfile;
	char *cmd;
	size_t len;

	helpfile = find_helpfile();
	if (helpfile == NULL)
	{
		error("Cannot find help file", NULL_PARG);
		return;
	}
#if !HAVE_SYSTEM
	/*
	 * Just examine the help file.
	 */
	(void) edit(helpfile);
#else
	/*
	 * Use lsystem() to invoke a new instance of less
	 * to view the help file.
	 */
#if MSDOS_COMPILER
	putenv("LESS=-m -H -+E -+s -PmHELP -- ?eEND -- Press g to see it again:Press RETURN for more., or q when done");
	len = strlen(helpfile) + strlen(progname) + 3;
	cmd = (char *) ecalloc(len, sizeof(char));
	snprintf(cmd, len, "-%s %s", progname, helpfile);
#else
	len = strlen(helpfile) + strlen(progname) + 150;
	cmd = (char *) ecalloc(len, sizeof(char));
#if OS2
	snprintf(cmd, len,
	 "-%s -m -H -+E -+s \"-PmHELP -- ?eEND -- Press g to see it again:Press RETURN for more., or q when done \" %s",
		progname, helpfile);
#else
	snprintf(cmd, len,
	 "-%s -m -H -+E -+s '-PmHELP -- ?eEND -- Press g to see it again:Press RETURN for more., or q when done ' %s",
		progname, helpfile);
#endif
#endif
	free(helpfile);
	lsystem(cmd, (char*)NULL);
	if (!nomsg)
		error("End of help", NULL_PARG);
	free(cmd);
#endif
#else
	error("Cannot find help file", NULL_PARG);
#endif
}
