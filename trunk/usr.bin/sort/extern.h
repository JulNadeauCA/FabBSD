/*	$OpenBSD: extern.h,v 1.7 2003/06/26 00:12:39 deraadt Exp $	*/

/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Peter McIlroy.
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
 *	@(#)extern.h	8.1 (Berkeley) 6/6/93
 */

void	 append(u_char **, int, int, FILE *, void (*)(RECHEADER *, FILE *),
	    struct field *);
void	 concat(FILE *, FILE *);
length_t enterkey(RECHEADER *, DBT *, int, struct field *);
void	 fixit(int *, char **);
void	 fldreset(struct field *);
FILE	*ftmp(void);
void	 fmerge(int, union f_handle, int,
	    int (*)(int, union f_handle, int, RECHEADER *, u_char *, struct field *),
	    FILE *, void (*)(RECHEADER *, FILE *), struct field *);
void	 fsort(int, int, union f_handle, int, FILE *, struct field *);
int	 geteasy(int, union f_handle,
	    int, RECHEADER *, u_char *, struct field *);
int	 getnext(int, union f_handle,
	    int, RECHEADER *, u_char *, struct field *);
int	 makekey(int, union f_handle,
	    int, RECHEADER *, u_char *, struct field *);
int	 makeline(int, union f_handle,
	    int, RECHEADER *, u_char *, struct field *);
void	 merge(int, int,
	    int (*)(int, union f_handle, int, RECHEADER *, u_char *, struct field *),
	    FILE *, void (*)(RECHEADER *, FILE *), struct field *);
void	 num_init(void);
void	 onepass(u_char **, int, long, long *, u_char *, FILE *);
int	 optval(int, int);
void	 order(union f_handle,
	    int (*)(int, union f_handle, int, RECHEADER *, u_char *, struct field *),
	    struct field *);
void	 putline(RECHEADER *, FILE *);
void	 putrec(RECHEADER *, FILE *);
void	 rd_append(int, union f_handle, int, FILE *, u_char *, u_char *);
int	 setfield(char *, struct field *, int);
void	 settables(int);
