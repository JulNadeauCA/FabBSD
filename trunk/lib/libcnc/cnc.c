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

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>

#include <cnc.h>

#include "pathnames.h"

char *cnc_error_msg = NULL;
struct cnc_velocity cnc_vel_default;	/* Default motion velocity */

/* Return a message string from the last error. */
const char *
cnc_get_error(void)
{
	return ((const char *)cnc_error_msg);
}

/* Set the error message string. */
void
cnc_set_error(const char *fmt, ...)
{
	va_list args;
	char *buf;
	
	va_start(args, fmt);
	if (vasprintf(&buf, fmt, args) != -1) {
		free(cnc_error_msg);
		cnc_error_msg = buf;
	}
	va_end(args);
}

/* Initialize libcnc */
int
cnc_init(void)
{
	struct cnc_config *cf;

	if ((cf = cnc_config_open(_PATH_CNC_CONF)) == NULL)
		return (-1);

	cnc_config_vel(cf, "default_v0", &cnc_vel_default.v0, 1000UL);
	cnc_config_vel(cf, "default_F", &cnc_vel_default.F, 250000UL);
	cnc_config_vel(cf, "default_Amax", &cnc_vel_default.Amax, 1000000000UL);
	cnc_config_vel(cf, "default_Jmax", &cnc_vel_default.Jmax, 1000000000UL);
	return (0);
}

/* Cleanup libcnc */
void
cnc_destroy(void)
{
}
