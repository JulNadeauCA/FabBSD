/*	$OpenBSD: wwinit.c,v 1.18 2007/09/02 15:19:35 deraadt Exp $	*/
/*	$NetBSD: wwinit.c,v 1.11 1996/02/08 21:49:07 mycroft Exp $	*/

/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Edward Wang at The University of California, Berkeley.
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
#if 0
static char sccsid[] = "@(#)wwinit.c	8.2 (Berkeley) 4/28/95";
#else
static char rcsid[] = "$OpenBSD: wwinit.c,v 1.18 2007/09/02 15:19:35 deraadt Exp $";
#endif
#endif /* not lint */

#include <stdlib.h>
#include "ww.h"
#include "tt.h"
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include "char.h"

wwinit()
{
	int i, j;
	char *kp;
	sigset_t sigset, osigset;

	wwdtablesize = 3;
	wwhead.ww_forw = &wwhead;
	wwhead.ww_back = &wwhead;

	sigemptyset(&sigset);
	sigaddset(&sigset, SIGCHLD);
	sigaddset(&sigset, SIGALRM);
	sigaddset(&sigset, SIGHUP);
	sigaddset(&sigset, SIGTERM);
	sigprocmask(SIG_BLOCK, &sigset, &osigset);

	if (signal(SIGCHLD, wwchild) == SIG_ERR ||
	    signal(SIGHUP, wwquit) == SIG_ERR ||
	    signal(SIGTERM, wwquit) == SIG_ERR ||
	    signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
		wwerrno = WWE_SYS;
		return -1;
	}

	if (wwgettty(0, &wwoldtty) < 0)
		return -1;
	wwwintty = wwoldtty;
#ifdef OLD_TTY
	wwwintty.ww_sgttyb.sg_flags &= ~XTABS;
	wwnewtty.ww_sgttyb = wwoldtty.ww_sgttyb;
	wwnewtty.ww_sgttyb.sg_erase = -1;
	wwnewtty.ww_sgttyb.sg_kill = -1;
	wwnewtty.ww_sgttyb.sg_flags |= CBREAK;
	wwnewtty.ww_sgttyb.sg_flags &= ~(ECHO|CRMOD);
	wwnewtty.ww_tchars.t_intrc = -1;
	wwnewtty.ww_tchars.t_quitc = -1;
	wwnewtty.ww_tchars.t_startc = -1;
	wwnewtty.ww_tchars.t_stopc = -1;
	wwnewtty.ww_tchars.t_eofc = -1;
	wwnewtty.ww_tchars.t_brkc = -1;
	wwnewtty.ww_ltchars.t_suspc = -1;
	wwnewtty.ww_ltchars.t_dsuspc = -1;
	wwnewtty.ww_ltchars.t_rprntc = -1;
	wwnewtty.ww_ltchars.t_flushc = -1;
	wwnewtty.ww_ltchars.t_werasc = -1;
	wwnewtty.ww_ltchars.t_lnextc = -1;
	wwnewtty.ww_lmode = wwoldtty.ww_lmode | LLITOUT;
	wwnewtty.ww_ldisc = wwoldtty.ww_ldisc;
#else
#ifndef OXTABS
#define OXTABS XTABS
#endif
#ifndef _POSIX_VDISABLE
#define _POSIX_VDISABLE -1
#endif
	wwwintty.ww_termios.c_oflag &= ~OXTABS;
	wwnewtty.ww_termios = wwoldtty.ww_termios;
	wwnewtty.ww_termios.c_iflag &=
		~(ISTRIP | INLCR | IGNCR | ICRNL | IXON | IXOFF | IMAXBEL);
	wwnewtty.ww_termios.c_oflag = 0;
	wwnewtty.ww_termios.c_cflag &= ~(CSIZE | PARENB);
	wwnewtty.ww_termios.c_cflag |= CS8;
	wwnewtty.ww_termios.c_lflag = 0;
	for (i = 0; i < NCCS; i++)
		wwnewtty.ww_termios.c_cc[i] = _POSIX_VDISABLE;
	wwnewtty.ww_termios.c_cc[VMIN] = 1;
	wwnewtty.ww_termios.c_cc[VTIME] = 0;
#endif
	if (wwsettty(0, &wwnewtty) < 0)
		goto bad;

	if ((wwterm = getenv("TERM")) == 0) {
		wwerrno = WWE_BADTERM;
		goto bad;
	}
#ifdef TERMINFO
	if (setupterm(wwterm, STDOUT_FILENO, NULL) != 0) {
		wwerrno = WWE_BADTERM;
		goto bad;
	}
#else
	if (tgetent(wwtermcap, wwterm) != 1) {
		wwerrno = WWE_BADTERM;
		goto bad;
	}
#endif
#ifdef OLD_TTY
	wwospeed = wwoldtty.ww_sgttyb.sg_ospeed;
#else
	wwospeed = cfgetospeed(&wwoldtty.ww_termios);
	wwbaud = wwospeed;
#endif
	switch (wwospeed) {
	default:
	case B0:
		wwbaud = 0;
		break;
	case B50:
		wwbaud = 50;
		break;
	case B75:
		wwbaud = 75;
		break;
	case B110:
		wwbaud = 110;
		break;
	case B134:
		wwbaud = 134;
		break;
	case B150:
		wwbaud = 150;
		break;
	case B200:
		wwbaud = 200;
		break;
	case B300:
		wwbaud = 300;
		break;
	case B600:
		wwbaud = 600;
		break;
	case B1200:
		wwbaud = 1200;
		break;
	case B1800:
		wwbaud = 1800;
		break;
	case B2400:
		wwbaud = 2400;
		break;
	case B4800:
		wwbaud = 4800;
		break;
	case B9600:
		wwbaud = 9600;
		break;
#ifdef B19200
	case B19200:
#else
	case EXTA:
#endif
		wwbaud = 19200;
		break;
#ifdef B38400
	case B38400:
#else
	case EXTB:
#endif
		wwbaud = 38400;
		break;
#ifdef B57600
	case B57600:
		wwbaud = 57600;
		break;
#endif
#ifdef B115200
	case B115200:
		wwbaud = 115200;
		break;
#endif
	}

	if (xxinit() < 0)
		goto bad;
	wwnrow = tt.tt_nrow;
	wwncol = tt.tt_ncol;
	wwavailmodes = tt.tt_availmodes;
	wwwrap = tt.tt_wrap;

	if (wwavailmodes & WWM_REV)
		wwcursormodes = WWM_REV | wwavailmodes & WWM_BLK;
	else if (wwavailmodes & WWM_UL)
		wwcursormodes = WWM_UL;

	if ((wwib = malloc(512)) == 0)
		goto bad;
	wwibe = wwib + 512;
	wwibq = wwibp = wwib;

	wwsmap = (unsigned char **)
		wwalloc(0, 0, wwnrow, wwncol, sizeof (unsigned char));
	if (wwsmap == 0)
		goto bad;
	for (i = 0; i < wwnrow; i++)
		for (j = 0; j < wwncol; j++)
			wwsmap[i][j] = WWX_NOBODY;

	wwos = (union ww_char **)
		wwalloc(0, 0, wwnrow, wwncol, sizeof (union ww_char));
	if (wwos == 0)
		goto bad;
	/* wwos is cleared in wwstart1() */
	wwns = (union ww_char **)
		wwalloc(0, 0, wwnrow, wwncol, sizeof (union ww_char));
	if (wwns == 0)
		goto bad;
	for (i = 0; i < wwnrow; i++)
		for (j = 0; j < wwncol; j++)
			wwns[i][j].c_w = ' ';
	if (tt.tt_checkpoint) {
		/* wwcs is also cleared in wwstart1() */
		wwcs = (union ww_char **)
			wwalloc(0, 0, wwnrow, wwncol, sizeof (union ww_char));
		if (wwcs == 0)
			goto bad;
	}

	wwtouched = malloc(wwnrow);
	if (wwtouched == 0) {
		wwerrno = WWE_NOMEM;
		goto bad;
	}
	for (i = 0; i < wwnrow; i++)
		wwtouched[i] = 0;

	wwupd = (struct ww_update *) calloc(wwnrow, sizeof *wwupd);
	if (wwupd == 0) {
		wwerrno = WWE_NOMEM;
		goto bad;
	}

	wwindex[WWX_NOBODY] = &wwnobody;
	wwnobody.ww_order = NWW;

	kp = wwwintermcap;
	if (wwavailmodes & WWM_REV)
		wwaddcap1(WWT_REV, &kp, wwwintermcap + sizeof wwwintermcap -
		    kp);
	if (wwavailmodes & WWM_BLK)
		wwaddcap1(WWT_BLK, &kp, wwwintermcap + sizeof wwwintermcap -
		    kp);
	if (wwavailmodes & WWM_UL)
		wwaddcap1(WWT_UL, &kp, wwwintermcap + sizeof wwwintermcap -
		    kp);
	if (wwavailmodes & WWM_GRP)
		wwaddcap1(WWT_GRP, &kp, wwwintermcap + sizeof wwwintermcap -
		    kp);
	if (wwavailmodes & WWM_DIM)
		wwaddcap1(WWT_DIM, &kp, wwwintermcap + sizeof wwwintermcap -
		    kp);
	if (wwavailmodes & WWM_USR)
		wwaddcap1(WWT_USR, &kp, wwwintermcap + sizeof wwwintermcap -
		    kp);
	if (tt.tt_insline && tt.tt_delline || tt.tt_setscroll)
		wwaddcap1(WWT_ALDL, &kp, wwwintermcap + sizeof wwwintermcap -
		    kp);
	if (tt.tt_inschar)
		wwaddcap1(WWT_IMEI, &kp, wwwintermcap + sizeof wwwintermcap -
		    kp);
	if (tt.tt_insspace)
		wwaddcap1(WWT_IC, &kp, wwwintermcap + sizeof wwwintermcap -
		    kp);
	if (tt.tt_delchar)
		wwaddcap1(WWT_DC, &kp, wwwintermcap + sizeof wwwintermcap -
		    kp);
	wwaddcap("kb", &kp, wwwintermcap + sizeof wwwintermcap - kp);
	wwaddcap("ku", &kp, wwwintermcap + sizeof wwwintermcap - kp);
	wwaddcap("kd", &kp, wwwintermcap + sizeof wwwintermcap - kp);
	wwaddcap("kl", &kp, wwwintermcap + sizeof wwwintermcap - kp);
	wwaddcap("kr", &kp, wwwintermcap + sizeof wwwintermcap - kp);
	wwaddcap("kh", &kp, wwwintermcap + sizeof wwwintermcap - kp);
	if ((j = tgetnum("kn")) >= 0) {
		char cap[32];

		(void) snprintf(kp, wwwintermcap + sizeof wwwintermcap - kp,
		    "kn#%d:", j);
		for (; *kp; kp++)
			;
		for (i = 1; i <= j; i++) {
			(void) snprintf(cap, sizeof(cap), "k%d", i);
			wwaddcap(cap, &kp, wwwintermcap +
			    sizeof wwwintermcap - kp);
			cap[0] = 'l';
			wwaddcap(cap, &kp, wwwintermcap +
			    sizeof wwwintermcap - kp);
		}
	}
	/*
	 * It's ok to do this here even if setenv() is destructive
	 * since tt_init() has already made its own copy of it and
	 * wwterm now points to the copy.
	 */
	(void) setenv("TERM", WWT_TERM, 1);
#ifdef TERMINFO
	if (wwterminfoinit() < 0)
		goto bad;
#endif

	if (tt.tt_checkpoint)
		if (signal(SIGALRM, wwalarm) == SIG_ERR) {
			wwerrno = WWE_SYS;
			goto bad;
		}
	wwstart1();

	sigprocmask(SIG_SETMASK, &osigset, (sigset_t *)0);
	return 0;

bad:
	/*
	 * Don't bother to free storage.  We're supposed
	 * to exit when wwinit fails anyway.
	 */
	(void) wwsettty(0, &wwoldtty);

	sigprocmask(SIG_SETMASK, &osigset, (sigset_t *)0);
	return -1;
}

wwaddcap(cap, kp, len)
	char *cap;
	char **kp;
	int len;
{
	char tbuf[1024];	/* tgetstr(, &tp) does strlcpy(tp,, 1024) */
	char *tp = tbuf;
	char *str, *p;

	if ((str = tgetstr(cap, &tp)) != 0) {
		int need = strlen(cap) + 3;

		for (p = str; *p; ++p)
			need += strlen(unctrl(*p));
		if (need > len)
			return;
		while (*(*kp)++ = *cap++)
			;
		(*kp)[-1] = '=';
		while (*str) {
			for (p = unctrl(*str++); *(*kp)++ = *p++;)
				;
			(*kp)--;
		}
		*(*kp)++ = ':';
		**kp = 0;
	}
}

wwaddcap1(cap, kp, len)
	char *cap;
	char **kp;
	int len;
{
	if (strlen(cap) + 1 > len)
		return;
	while (*(*kp)++ = *cap++)
		;
	(*kp)--;
}

wwstart()
{
	int i;

	(void) wwsettty(0, &wwnewtty);
	for (i = 0; i < wwnrow; i++)
		wwtouched[i] = WWU_TOUCHED;
	wwstart1();
}

wwstart1()
{
	int i, j;

	for (i = 0; i < wwnrow; i++)
		for (j = 0; j < wwncol; j++) {
			wwos[i][j].c_w = ' ';
			if (tt.tt_checkpoint)
				wwcs[i][j].c_w = ' ';
		}
	xxstart();
	if (tt.tt_checkpoint)
		wwdocheckpoint = 1;
}

/*
 * Reset data structures and terminal from an unknown state.
 * Restoring wwos has been taken care of elsewhere.
 */
wwreset()
{
	int i;

	xxreset();
	for (i = 0; i < wwnrow; i++)
		wwtouched[i] = WWU_TOUCHED;
}
