/*	$FabBSD$	*/
/*
 * Copyright (c) 2009 Hypertriton, Inc. <http://hypertriton.com/>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/limits.h>

#include <limits.h>
#include <cnc.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

/* Parse a string representing a distance. */
int
cnc_dist_parse(cnc_dist_t *dist, const char *s)
{
	const cnc_unit_t *unit;
	char *ep;

	errno = 0;
	*dist = strtoul(s, &ep, 10);
	if (s[0] == '\0') {
		cnc_set_error("No numerical distance value");
		return (-1);
	}
	if (errno == ERANGE || *dist == ULONG_MAX) {
		cnc_set_error("Distance value out of range");
		return (-1);
	}
	if (ep != '\0') {
		if ((unit = cnc_find_unit(ep, cncLengthUnits)) == NULL) {
			return (-1);
		}
		*dist *= unit->divider;
	}
	return (0);
}

/* Parse a string representing an angle. */
int
cnc_angle_parse(int *angle, const char *s)
{
	const cnc_unit_t *unit;
	char *ep;

	errno = 0;
	*angle = strtoul(s, &ep, 10);
	if (s[0] == '\0') {
		cnc_set_error("No numerical angle value");
		return (-1);
	}
	if (errno == ERANGE || *angle == ULONG_MAX) {
		cnc_set_error("Angle value out of range");
		return (-1);
	}
	if (ep != '\0') {
		if ((unit = cnc_find_unit(ep, cncAngleUnits)) == NULL) {
			return (-1);
		}
		*angle *= unit->divider;
	}
	return (0);
}
