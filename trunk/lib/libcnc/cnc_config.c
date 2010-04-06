/*
 * Copyright (c) 2008-2010 Hypertriton, Inc. <http://hypertriton.com/>
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

/*
 * Configuration file loading.
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <stdarg.h>

#include <cnc.h>
#include "pathnames.h"

struct cnc_config *
cnc_config_open(const char *file)
{
	char path[FILENAME_MAX];
	struct cnc_config *cf;
	char *data, *s, *line;
	size_t len, nRead;
	ssize_t rv;
	int fd;

	if (file == NULL) {
		strlcpy(path, _PATH_CNC_CONF, sizeof(path));
	} else {
		strlcpy(path, file, sizeof(path));
	}
	if ((fd = open(path, O_RDONLY)) == -1) {
		cnc_set_error("%s: %s", path, strerror(errno));
		return (NULL);
	}
	if ((len = (size_t)lseek(fd, 0, SEEK_END)) == 0) {
		if ((cf = malloc(sizeof(struct cnc_config))) == NULL) {
			cnc_set_error("Out of memory");
			close(fd);
			return (NULL);
		}
		cf->keys = NULL;
		cf->vals = NULL;
		cf->nvals = 0;
		return (cf);
	}
	lseek(fd, 0, SEEK_SET);
	if ((data = malloc(len)) == NULL) {
		close(fd);
		return (NULL);
	}
	for (nRead = 0; nRead < len; ) {
		rv = read(fd, &data[nRead], len-nRead);
		if (rv == -1) {
			if (errno == EINTR) { continue; }
			goto fail_read;
		} else if (rv == 0) {
			goto fail_read;
		}
		nRead += rv;
	}
	close(fd);

	if ((cf = malloc(sizeof(struct cnc_config))) == NULL) {
		goto outofmem;
	}
	if ((cf->keys = malloc(sizeof(char **))) == NULL) {
		free(cf);
		goto outofmem;
	}
	if ((cf->vals = malloc(sizeof(char **))) == NULL) {
		free(cf->keys);
		free(cf);
		goto outofmem;
	}
	cf->nvals = 0;

	s = data;
	while ((line = strsep(&s, "\n")) != NULL) {
		char *c = &line[0], *cKey, *cVal;

		while (isspace(*c)) {
			c++;
		}
		if (*c == '#')
			continue; 

		cKey = strsep(&c, "=");
		cVal = strsep(&c, "=");

		if (cKey == NULL || cVal == NULL)
			continue;

		while (isspace(*cKey)) { cKey++; }
		while (isspace(*cVal)) { cVal++; }

		len = strlen(cKey)-1;
		for (;;) {
			if (!isspace(cKey[len])) {
				break;
			}
			cKey[len] = '\0';
			len--;
		}

		cf->keys = realloc(cf->keys, (cf->nvals+1)*sizeof(char *));
		cf->vals = realloc(cf->vals, (cf->nvals+1)*sizeof(char *));
		if (cf->keys == NULL || cf->vals == NULL) {
			goto outofmem;
		}
		if ((cf->keys[cf->nvals] = strdup(cKey)) == NULL) {
			goto outofmem;
		}
		if ((cf->vals[cf->nvals] = strdup(cVal)) == NULL) {
			goto outofmem;
		}
		cf->nvals++;
	}

	free(data);
	return (cf);
fail_read:
	cnc_set_error("Read error");
	close(fd);
	free(data);
	return (NULL);
outofmem:
	cnc_set_error("Out of memory");
	free(data);
	return (NULL);
}

void
cnc_config_close(struct cnc_config *cf)
{
	free(cf->keys);
	free(cf->vals);
	free(cf);
}

void
cnc_config_int(struct cnc_config *cf, const char *key, int *pVal, int dVal)
{
	u_int i;
	char *val;
	
	for (i = 0; i < cf->nvals; i++) {
		if (!strcasecmp(cf->keys[i], key)) {
			val = cf->vals[i];
			if (!strcasecmp(val, "true") ||
			    !strcasecmp(val, "yes") ||
			    !strcasecmp(val, "on")) {
				*pVal = 1;
			} else if (!strcasecmp(val, "false") ||
			           !strcasecmp(val, "no") ||
			           !strcasecmp(val, "off")) {
				*pVal = 0;
			} else {
				*pVal = (int)strtol(cf->vals[i], NULL, 10);
			}
			return;
		}
	}
	*pVal = dVal;
}

void
cnc_config_uint(struct cnc_config *cf, const char *key, u_int *pVal, u_int dVal)
{
	u_int i;
	
	for (i = 0; i < cf->nvals; i++) {
		if (strcasecmp(cf->keys[i], key) == 0) {
			*pVal = (u_int)strtoul(cf->vals[i], NULL, 10);
			return;
		}
	}
	*pVal = dVal;
}

void
cnc_config_long(struct cnc_config *cf, const char *key, long *pVal, long dVal)
{
	u_int i;
	
	for (i = 0; i < cf->nvals; i++) {
		if (strcasecmp(cf->keys[i], key) == 0) {
			*pVal = (long)strtoul(cf->vals[i], NULL, 10);
			return;
		}
	}
	*pVal = dVal;
}

void
cnc_config_ulong(struct cnc_config *cf, const char *key, u_long *pVal, u_long dVal)
{
	u_int i;
	
	for (i = 0; i < cf->nvals; i++) {
		if (strcasecmp(cf->keys[i], key) == 0) {
			*pVal = strtoul(cf->vals[i], NULL, 10);
			return;
		}
	}
	*pVal = dVal;
}

void
cnc_config_flt(struct cnc_config *cf, const char *key, float *pVal, float dVal)
{
	u_int i;
	
	for (i = 0; i < cf->nvals; i++) {
		if (strcasecmp(cf->keys[i], key) == 0) {
			*pVal = (float)strtod(cf->vals[i], NULL);
			return;
		}
	}
	*pVal = (float)dVal;
}

void
cnc_config_dbl(struct cnc_config *cf, const char *key, double *pVal, double dVal)
{
	u_int i;
	
	for (i = 0; i < cf->nvals; i++) {
		if (strcasecmp(cf->keys[i], key) == 0) {
			*pVal = strtod(cf->vals[i], NULL);
			return;
		}
	}
	*pVal = dVal;
}

void
cnc_config_string(struct cnc_config *cf, const char *key, char **pVal,
    const char *dVal)
{
	u_int i;
	size_t len;
	char *s;
	
	for (i = 0; i < cf->nvals; i++) {
		if (strcasecmp(cf->keys[i], key) == 0) {
			if ((s = strdup(cf->vals[i])) == NULL) {
				cnc_set_error("Out of memory");
				*pVal = NULL;
				return;
			}
			if (*s == '"' || *s == '\'') { s++; }
			len = strlen(s);
			if (s[len-1] == '"' || s[len-1] == '\'') {
				s[len-1] = '\0';
			}
			*pVal = s;
			return;
		}
	}
	if ((*pVal = strdup(dVal)) == NULL) {
		cnc_set_error("Out of memory");
		*pVal = NULL;
	}
}

void
cnc_config_strlcpy(struct cnc_config *cf, const char *key, char *dst,
    size_t len, const char *dVal)
{
	u_int i;
	size_t sLen;
	
	for (i = 0; i < cf->nvals; i++) {
		if (strcasecmp(cf->keys[i], key) == 0) {
			char *s = cf->vals[i];

			if (*s == '"' || *s == '\'') { s++; }
			strlcpy(dst, s, len);
			sLen = strlen(dst);
			if (dst[sLen-1] == '"' || dst[sLen-1] == '\'') {
				dst[sLen-1] = '\0';
			}
			return;
		}
	}
	strlcpy(dst, dVal, len);
}
