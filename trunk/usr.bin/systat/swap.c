/*	$OpenBSD: swap.c,v 1.21 2008/06/12 22:26:01 canacar Exp $	*/
/*	$NetBSD: swap.c,v 1.9 1998/12/26 07:05:08 marc Exp $	*/

/*-
 * Copyright (c) 1997 Matthew R. Green.  All rights reserved.
 * Copyright (c) 1980, 1992, 1993
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

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/swap.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "systat.h"


static	long blocksize;
static	int hlen, nswap, rnswap;
static	struct swapent *swap_devices;

void print_sw(void);
int read_sw(void);
int select_sw(void);
static void showswap(int i);
static void showtotal(void);


field_def fields_sw[] = {
	{"DISK", 6, 16, 1, FLD_ALIGN_LEFT, -1, 0, 0, 0},
	{"BLOCKS", 5, 10, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"USED", 5, 10, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"", 40, 80, 1, FLD_ALIGN_BAR, -1, 0, 0, 100},
};

#define FIELD_ADDR(x) (&fields_sw[x])

#define FLD_SW_NAME	FIELD_ADDR(0)
#define FLD_SW_BLOCKS	FIELD_ADDR(1)
#define FLD_SW_USED	FIELD_ADDR(2)
#define FLD_SW_BAR	FIELD_ADDR(3)

/* Define views */
field_def *view_sw_0[] = {
	FLD_SW_NAME, FLD_SW_BLOCKS, FLD_SW_USED, FLD_SW_BAR, NULL
};


/* Define view managers */
struct view_manager swap_mgr = {
	"Swap", select_sw, read_sw, NULL, print_header,
	print_sw, keyboard_callback, NULL, NULL
};

field_view views_sw[] = {
	{view_sw_0, "swap", '6', &swap_mgr},
	{NULL, NULL, 0, NULL}
};


int
select_sw(void)
{
	if (swap_devices == NULL || nswap == 0)
		num_disp = 1;
	else
		num_disp = nswap;
	if (nswap > 1)
		num_disp++;
	return (0);
}

int
read_sw(void)
{
	num_disp = 1;

	nswap = swapctl(SWAP_NSWAP, 0, 0);

	if (nswap < 0)
		error("error: %s", strerror(errno));
	if (nswap == 0)
		return 0;

	if (swap_devices)
		(void)free(swap_devices);

	swap_devices = (struct swapent *)calloc(nswap, sizeof(*swap_devices));
	if (swap_devices == NULL)
		return 0;

	rnswap = swapctl(SWAP_STATS, (void *)swap_devices, nswap);
	if (rnswap < 0 || nswap != rnswap)
		return 0;

	num_disp = nswap;
	if (nswap > 1)
		num_disp++;

	return 0;
}


void
print_sw(void)
{
	int n, count = 0;

	if (swap_devices == NULL || nswap == 0) {
		print_fld_str(FLD_SW_BAR, "No swap devices");
		return;
	}


	for (n = dispstart; n < num_disp; n++) {
		if (n >= nswap)
			showtotal();
		else
			showswap(n);
		count++;
		if (maxprint > 0 && count >= maxprint)
			break;
	}

}

int
initswap(void)
{
	field_view *v;

	char *bs = getbsize(&hlen, &blocksize);

	FLD_SW_BLOCKS->title = strdup(bs);

	for (v = views_sw; v->name != NULL; v++)
		add_view(v);

	return(1);
}


static void
showswap(int i)
{
	int div, used, xsize;
	struct	swapent *sep;
	char	*p;

	div = blocksize / 512;

	sep = &swap_devices[i];

	p = strrchr(sep->se_path, '/');
	p = p ? p+1 : sep->se_path;

	print_fld_str(FLD_SW_NAME, p);
	
	xsize = sep->se_nblks;
	used = sep->se_inuse;

	print_fld_uint(FLD_SW_BLOCKS, xsize / div);
	print_fld_uint(FLD_SW_USED, used / div);
	print_fld_bar(FLD_SW_BAR, 100 * used / xsize);

	end_line();
}

static void
showtotal(void)
{
	struct	swapent *sep;
	int	div, i, avail, used, xsize, free;

	div = blocksize / 512;
	free = avail = 0;

	for (sep = swap_devices, i = 0; i < nswap; i++, sep++) {
		xsize = sep->se_nblks;
		used = sep->se_inuse;
		avail += xsize;
		free += xsize - used;
	}
	used = avail - free;

	print_fld_str(FLD_SW_NAME, "Total");
	print_fld_uint(FLD_SW_BLOCKS, avail / div);
	print_fld_uint(FLD_SW_USED, used / div);
	print_fld_bar(FLD_SW_BAR, 100 * used / avail);

	end_line();
}
