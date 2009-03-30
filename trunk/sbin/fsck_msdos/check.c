/*	$OpenBSD: check.c,v 1.11 2006/05/27 22:30:09 thib Exp $	*/
/*	$NetBSD: check.c,v 1.8 1997/10/17 11:19:29 ws Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997 Wolfgang Solfrank
 * Copyright (c) 1995 Martin Husemann
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Martin Husemann
 *	and Wolfgang Solfrank.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef lint
static char rcsid[] = "$OpenBSD: check.c,v 1.11 2006/05/27 22:30:09 thib Exp $";
#endif /* not lint */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include "ext.h"

int
checkfilesys(const char *fname)
{
	int dosfs;
	struct bootblock boot;
	struct fatEntry *fat = NULL;
	int i;
	int mod = 0;

	rdonly = alwaysno;
	if (!preen)
		printf("** %s", fname);

	dosfs = open(fname, rdonly ? O_RDONLY : O_RDWR, 0);
	if (dosfs < 0 && !rdonly) {
		dosfs = open(fname, O_RDONLY, 0);
		if (dosfs >= 0)
			pwarn(" (NO WRITE)\n");
		else if (!preen)
			printf("\n");
		rdonly = 1;
	} else if (!preen)
		printf("\n");

	if (dosfs < 0) {
		xperror("Can't open");
		return (8);
	}

	if (readboot(dosfs, &boot) != FSOK) {
		(void)close(dosfs);
		return (8);
	}

	if (!preen) {
		if (boot.ValidFat < 0)
			printf("** Phase 1 - Read and Compare FATs\n");
		else
			printf("** Phase 1 - Read FAT\n");
	}

	mod |= readfat(dosfs, &boot, boot.ValidFat >= 0 ? boot.ValidFat : 0, &fat);
	if (mod & FSFATAL) {
		(void)close(dosfs);
		return 8;
	}

	if (boot.ValidFat < 0)
		for (i = 1; i < boot.FATs; i++) {
			struct fatEntry *currentFat;
			mod |= readfat(dosfs, &boot, i, &currentFat);

			if (mod & FSFATAL) {
				free(fat);
				(void)close(dosfs);
				return 8;
			}

			mod |= comparefat(&boot, fat, currentFat, i);
			free(currentFat);
			if (mod & FSFATAL) {
				free(fat);
				(void)close(dosfs);
				return (8);
			}
		}

	if (!preen)
		printf("** Phase 2 - Check Cluster Chains\n");

	mod |= checkfat(&boot, fat);
	if (mod & FSFATAL) {
		free(fat);
		(void)close(dosfs);
		return (8);
	}

	if (mod & FSFATMOD)
		mod |= writefat(dosfs, &boot, fat); /* delay writing fats?	XXX */
	if (mod & FSFATAL) {
		free(fat);
		(void)close(dosfs);
		return (8);
	}

	if (!preen)
		printf("** Phase 3 - Check Directories\n");

	mod |= resetDosDirSection(&boot, fat);
	if (mod & FSFATAL) {
		free(fat);
		close(dosfs);
		return 8;
	}

	if (mod & FSFATMOD)
		mod |= writefat(dosfs, &boot, fat); /* delay writing fats?	XXX */
	if (mod & FSFATAL) {
		finishDosDirSection();
		free(fat);
		(void)close(dosfs);
		return (8);
	}

	mod |= handleDirTree(dosfs, &boot, fat);
	if (mod & FSFATAL) {
		finishDosDirSection();
		free(fat);
		(void)close(dosfs);
		return (8);
	}

	if (!preen)
		printf("** Phase 4 - Check for Lost Files\n");

	mod |= checklost(dosfs, &boot, fat);
	if (mod & FSFATAL) {
		finishDosDirSection();
		free(fat);
		(void)close(dosfs);
		return 8;
	}

	if (mod & FSFATMOD)
		mod |= writefat(dosfs, &boot, fat); /* delay writing fats?    XXX */

	finishDosDirSection();
	free(fat);
	(void)close(dosfs);
	if (mod & FSFATAL)
		return 8;

	if (boot.NumBad)
		pwarn("%d files, %d free (%d clusters), %d bad (%d clusters)\n",
		      boot.NumFiles,
		      boot.NumFree * boot.ClusterSize / 1024, boot.NumFree,
		      boot.NumBad * boot.ClusterSize / 1024, boot.NumBad);
	else
		pwarn("%d files, %d free (%d clusters)\n",
		      boot.NumFiles,
		      boot.NumFree * boot.ClusterSize / 1024, boot.NumFree);

	if (mod & (FSFATAL | FSERROR))
		return (8);
	if (mod) {
		pwarn("\n***** FILE SYSTEM WAS MODIFIED *****\n");
		return (4);
	}
	return (0);
}
