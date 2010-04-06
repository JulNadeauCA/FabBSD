/*	$FabBSD$	*/
/*
 * Copyright (c) 2009-2010 Hypertriton, Inc. <http://hypertriton.com/>
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
 * RS274/NGC control code interpreter.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <err.h>

#include "ngc.h"
#include "pathnames.h"

int	ngc_verbose = 1;
int	ngc_warnings = 0;
int	ngc_simulate = 0;

struct ngc_progq	 ngc_progs;
ngc_real_t		 ngc_params[NGC_NPARAMS];
int			 ngc_csys_bases[10] = { 0, 5221, 5241, 5261, 5281,
			                        5301, 5321, 5341, 5361, 5381 };
ngc_real_t		 ngc_csys_offset[6];

#include "addresses.h"
#include "modal.h"

ngc_vec_t		ngc_pos;
ngc_real_t		ngc_linscale = 400.0;	/* mm (XYZ) to steps */
ngc_real_t		ngc_rotscale = 100.0;	/* degs (ABC) to steps */
ngc_real_t		ngc_feedscale = 100.0;	/* mm/min to steps/sec */

const int   ngc_naxes = 6;
const char *ngc_axis_words = "XYZABC";
const enum ngc_axis_type ngc_axis_types[6] = {
	NGC_LINEAR, NGC_LINEAR, NGC_LINEAR,
	NGC_ROTARY, NGC_ROTARY, NGC_ROTARY
};

enum ngc_interp_mode	ngc_mode = NGC_MILLING;
enum ngc_distance_mode	ngc_distmode = NGC_INCREMENTAL;
enum ngc_unit_system	ngc_units = NGC_METRIC;
enum ngc_feedrate_mode	ngc_feedmode = NGC_UNITS_PER_MIN;
ngc_real_t		ngc_feedfast = 100.0;	/* mm/min */
ngc_real_t		ngc_feedrate = 10.0;	/* mm/min or inverse time */
enum ngc_plane_mode	ngc_planesel = NGC_PLANE_XY;
enum ngc_cro_mode	ngc_cromode = NGC_CUTTER_OFFS_OFF;
int			ngc_tlo = 0;
enum ngc_pathctl_mode	ngc_pathctl = NGC_EXACT_PATH;
enum ngc_retract_mode	ngc_retract = NGC_RETRACT_PREVIOUS;

/* G-commands in effect */
int ngc_Gmodes[NGC_G_LAST] = {
	-1,
	80,	/* 1. G80: Cancel motion */
	17,	/* 2. G17: X-Y plane */
	91,	/* 3. G91: Incremental */
	-1,	/* 4 */
	94,	/* 5. G95: Units/minute mode */
	21,	/* 6. G21: Millimeters input */
	40,	/* 7. G40: Cutter radius compensation off */
	49,	/* 8. G49: Tool length offset off */
	-1,	/* 9 */
	98,	/* 10. G98: Retract to previous position */
	-1,	/* 11 */
	54,	/* 12. G54: Use coordinate system #1 */
	61	/* 13. G61: Exact path mode */
};

/* M-commands in effect */
int ngc_Mmodes[NGC_G_LAST] = {
	-1,	/* 0 */
	-1,	/* 1 */
	-1,	/* 2 */
	-1,	/* 3 */
	-1,	/* 4. Stopping */
	-1,	/* 5. Set I/O point */
	-1,	/* 6. Tool changer commands */
	-1,	/* 7. Spindle commands */
	-1,	/* 8. Coolant commands */
	-1,	/* 9. Panel switch overrides */
	-1,	/* 10 */
	-1,	/* 11 */
	-1,	/* 12 */
	-1,	/* 13 */
};

int	ngc_spinspeed = 0;
int	ngc_tool = 0;
int	ngc_csys = 1;

/*
 * Parse a RS274/NGC program line (or "block") into a list of ngc_words.
 * Verify that the addresses and values are appropriate to the current
 * interpreter mode.
 */
static int
ngc_parse_line(struct ngc_prog *prog, int nLine, char *pLine)
{
	struct ngc_block *blk;
	struct ngc_word *word = NULL;
	char *s, *line = pLine , *ep;
	int code, nWord = 0;

	if ((blk = realloc(prog->blks, (prog->nBlks+1)*sizeof(struct ngc_block)))
	    == NULL) {
		cnc_set_error("Out of memory");
		return (-1);
	}
	prog->blks = blk;
	blk = &prog->blks[prog->nBlks++];
	blk->n = 0;
	blk->line = nLine;
	blk->flags = 0;
	blk->prog = prog;
	TAILQ_INIT(&blk->words);

	if (line[0] == '/') {
		blk->flags |= NGC_BLOCK_OPTIONAL_DELETE;
		line++;
	}
	for (s = &line[0]; *s != '\0'; ) {
		const struct ngc_address *addr;
		int extn;

		if (isspace(*s)) {			/* Ignore spaces */
			s++;
			continue;
		}
		if (!isalpha(*s) || s[1] == '\0') {
			goto syntax;
		}
		code = toupper(*s);
		if (code < 'A' || code > 'Z') {
			goto syntax;
		}
		addr = &ngc_addr[code-0x41];
		if (addr->type == NGC_UNUSED) {
			cnc_set_error("Invalid address for %s mode: %c",
			    ngc_modenames[ngc_mode], code);
			goto fail;
		}
		if ((word = malloc(sizeof(struct ngc_word))) == NULL) {
			cnc_set_error("Out of memory");
			goto fail;
		}
		word->type = code;
		switch (addr->type) {
		case NGC_INTEGER:
			word->data.i = (int)strtoul(&s[1], &ep, 10);
			/* Translate [GM]x.y -> [GM]x*1000+y */
			if (*ep == '.' &&
			    ((code == 'G') || (code == 'M'))) {
				if (!isdigit(ep[1])) {
					goto syntax;
				}
				extn = (int)strtoul(&ep[1], NULL, 10);
				if (extn < 0 || extn > 1000) {
					goto syntax;
				}
				word->data.i = word->data.i*1000 + extn;
			}
			break;
		case NGC_SIGNED_REAL:
			word->data.f = (ngc_real_t)strtod(&s[1], &ep);
			break;
		case NGC_UNSIGNED_REAL:
			if (s[1] == '-') {
				cnc_set_error("%c values are unsigned",
				    code);
				goto fail;
			}
			word->data.f = (ngc_real_t)strtod(&s[1], &ep);
			break;
		default:
			break;
		}

		/* Validate program and block numbers. */
		switch (code) {
		case 'N':
			if (nWord > 0) {
				goto syntax;
			}
			if (word->data.i == 0) {
				cnc_set_error("Reserved block#: 0");
				goto fail;
			}
			blk->n = word->data.i;
			/*
			 * TODO: Warn of duplicates and non-incremental
			 * sequences.
			 */
			break;
		case ':':				/* ISO format */
		case 'O':				/* EIA format */
			if (nWord > 0) {
				goto syntax;
			}
			if (word->data.i == 0) {
				cnc_set_error("Reserved program#: 0");
				goto fail;
			}
			if (prog->name != 0) {
				cnc_set_error("Duplicate `%c' tag", code);
				goto fail;
			}
			prog->name = word->data.i;
			/*
			 * TODO: Warn of duplicates.
			 */
			break;
		}

		TAILQ_INSERT_TAIL(&blk->words, word, words);

		if (*ep == '\0') {
			break;
		}
		s = &ep[1];
		nWord++;
	}
	return (0);
syntax:
	cnc_set_error("Syntax error near \"%s\"", pLine);
fail:
	if (word != NULL) { free(word); }
	free(blk);
	return (-1);
}

/* Create a new ngc_program. */
struct ngc_prog *
ngc_prog_new(int name, const char *path)
{
	struct ngc_prog *prog;

	if ((prog = malloc(sizeof(struct ngc_prog))) == NULL) {
		cnc_set_error("Out of memory");
		return (NULL);
	}
	if (strcmp(path, "-") == 0) {
		strlcpy(prog->path, "(standard input)", sizeof(prog->path));
	} else {
		strlcpy(prog->path, path, sizeof(prog->path));
	}
	prog->name = name;
	prog->nRefs = 0;
	prog->nCalls = 0;
	prog->blks = NULL;
	prog->nBlks = 0;
	TAILQ_INSERT_TAIL(&ngc_progs, prog, progs);
	return (prog);
}

/* Destroy a ngc_program assuming it is not referenced. */
void
ngc_prog_del(struct ngc_prog *prog)
{
	TAILQ_REMOVE(&ngc_progs, prog, progs);
	free(prog->blks);
	free(prog);
}

/*
 * Parse the RS274/NGC program(s) contained in the specified file. Programs
 * are separated by '%' and identified by 'O' or ':' directives.
 */
static int
ngc_parse_file(const char *path)
{
	struct ngc_prog *prog = NULL;
	char lineBuf[NGC_LINE_MAX+1], *pLine, *line;
	FILE *f;
	size_t len;
	int nLine = 1, nProg = 0;
	int inComment = 0;
	char *s, *end;

	if (strcmp(path, "-") == 0) {
		f = fdopen(STDIN_FILENO, "r");
	} else {
		f = fopen(path, "r");
	}
	if (f == NULL) {
		cnc_set_error(" %s", strerror(errno));
		return (-1);
	}
	clearerr(f);
	while ((pLine = fgetln(f, &len))) {
		if (len >= NGC_LINE_MAX) {
			cnc_set_error("Line too long");
			goto fail;
		}
		memcpy(lineBuf, pLine, len);
		ngc_nltonul(lineBuf, len);

		/* Ignore leading whitespace and blank lines */
		for (line = &lineBuf[0]; isspace(*line); line++)
			;;
		if (line[0] == '\0')
			goto next_line;

		/* Look for the "%" demarcation sign */
		if (line[0] == '%') {
			if (prog != NULL) {
				prog = NULL;
				nProg++;
			}
			if ((prog = ngc_prog_new(0, path)) == NULL) {
				goto fail;
			}
			goto next_line;
		} else if (prog == NULL) {
			/* Begin program without "%" */
			if ((prog = ngc_prog_new(0, path)) == NULL)
				goto fail;
		}

		/* Strip comments and whitespace */
		if (inComment) {
			if ((s = strchr(line, ')')) == NULL) {
				goto next_line;
			}
			inComment = 0;
			line = &s[1];
		} else {
			s = &line[0];
			len = strlen(line);
			end = NULL;
			while ((s = strchr(line, '(')) != NULL) {
				if ((end = strchr(s, ')')) == NULL) {
					inComment = 1;		/* Multiline */
					goto next_line;
				}
				memmove(s, end+1, len-(end-s));
			}
		}
		for (;;) {
			len = strlen(line);
			if (isspace(line[len-1]))
				line[len-1] = '\0';
			else
				break;
		}
		if (line[0] == '\0')
			goto next_line;

		if (prog != NULL) {
			if (ngc_parse_line(prog, nLine, line) == -1)
				goto fail;
		}
next_line:
		nLine++;
	}
	if (ferror(f)) {
		cnc_set_error("Read error");
		goto fail;
	}
	fclose(f);
	return (0);
fail:
	cnc_set_error("%d: %s", nLine, cnc_get_error());
	fclose(f);
	return (-1);
}

static __dead void
printusage(void)
{
	extern char *__progname;
	printf("Usage: %s [-lmnvW] [-t toolfile] [file] ...]\n",
	    __progname);
	exit(1);
}

/* Issue a warning if warnings are enabled. */
void
warning(const char *fmt, ...)
{
	va_list args;

	if (!ngc_warnings)
		return;

	va_start(args, fmt);
	printf("WARNING: ");
	vprintf(fmt, args);
	va_end(args);
}

/* Issue a message in verbose mode. */
void
verbose(const char *fmt, ...)
{
	va_list args;

	if (!ngc_verbose)
		return;

	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
}

/* Restore the RS274/NGC persistent parameters from file. */
static void
loadparams(const char *path)
{
	char lineBuf[128], *pLine, *line, nLine = 1;
	char *key, *val, *s, *ep;
	const char *errstr = NULL;
	int param;
	size_t len;
	FILE *f;

	if ((f = fopen(path, "r")) == NULL) {
		err(1, "%s", path);
	}
	clearerr(f);
	while ((pLine = fgetln(f, &len))) {
		if (len > 128) { len = 128; }
		memcpy(lineBuf, pLine, len);
		ngc_nltonul(lineBuf, len);
		
		/* Ignore comments and whitespace */
		for (line = &lineBuf[0]; isspace(*line); line++)
			;;
		if ((s = strchr(line, '#')) != NULL)
			*s = '\0';
		if (line[0] == '\0')
			goto next_line;

		/* Read entry. */
		key = strsep(&line, " \t");
		val = strsep(&line, " \t");
		if (key == NULL || val == NULL)
			errx(1, "%s:%d: Syntax error", path, nLine);

		param = (int)strtonum(key, 1, 5400, &errstr);
		if (errstr != NULL) {
			errx(1, "%s:%d: %s", path, nLine, errstr);
		}
		ngc_params[param] = (ngc_real_t)strtod(val, &ep);
		if (*ep != '\0')
			errx(1, "%s:%d: Syntax error near \"%s\"", path,
			    nLine, ep);
next_line:
		nLine++;
	}
	fclose(f);

	/* We use a separate variable for coordinate system index. */
	ngc_csys = (int)ngc_params[5220];
	if (ngc_csys < 1 || ngc_csys >= NGC_NWORKOFFS)
		errx(1, "%s: illegal work offset %d (#5220)", path, ngc_csys);

	if (verbose) {
		verbose("Current G28 home: %.4f,%.4f,%.4f\n",
		    ngc_params[5161], ngc_params[5162], ngc_params[5163]);
		verbose("Current G30 home: %.4f,%.4f,%.4f\n",
		    ngc_params[5181], ngc_params[5182], ngc_params[5183]);
		verbose("Current G92 offset: %.4f,%.4f,%.4f\n",
		    ngc_params[5211], ngc_params[5212], ngc_params[5213]);
		verbose("Selected Coordinate System: %d\n", ngc_csys);
		verbose("Work offset #1: %.4f,%.4f,%.4f\n",
		    ngc_params[5221], ngc_params[5222], ngc_params[5223]);
	}
}

/*
 * Fetch the current integer position from the kernel and update ngc_pos
 * per the effective scaling factors. ngc can handle a maximum of 6 axes.
 */
static void
loadposition(void)
{
	cnc_vec_t cPos;
	int i;

	cnc_vec_zero(&cPos);
	if (cncpos(&cPos) == -1) {
		errx(1, "cncpos: %s", strerror(errno));
	}
	if (CNC_NAXES != NGC_NAXES) {
		warning("Kernel compiled for %d-axis support (RS274NGC "
		        "supports %d)\n", CNC_NAXES, NGC_NAXES);
	}
	for (i = 0; i < NGC_NAXES; i++)
		ngc_pos.v[i] = 0.0;
	for (i = 0; i < MIN(NGC_NAXES,CNC_NAXES); i++)
		ngc_pos.v[i] = (ngc_real_t)cPos.v[i] /
		               ((i>=3) ? ngc_linscale : ngc_rotscale);

	if (ngc_verbose) {
		char s[64];
		ngc_vec_print(s, sizeof(s), &ngc_pos);
		verbose("Current position: %s\n", s);
	}
}

int
main(int argc, char *argv[])
{
	char *toolfile = NULL;
	struct ngc_prog *prog, *progNext;
	int i, ch;

	TAILQ_INIT(&ngc_progs);

	/* Load the persistent numerical parameters. */
	for (i = 1; i < NGC_NPARAMS; i++) {
		ngc_params[i] = 0.0;
	}
	for (i = 0; i < 6; i++) {
		ngc_csys_offset[i] = 0.0;
	}
	loadparams(_PATH_NGC_PARAMS);

	while ((ch = getopt(argc, argv, "lmt:nvW?h")) != -1) {
		switch (ch) {
		case 'l':
			ngc_mode = NGC_TURNING;
			break;
		case 'm':
			break;
		case 't':
			if ((toolfile = strdup(optarg)) == NULL) {
				errx(1, "Out of memory");
			}
			break;
		case 'n':
			ngc_simulate = 1;
			break;
		case 'v':
			ngc_verbose = 1;
			break;
		case 'W':
			ngc_warnings = 1;
			break;
		default:
			printusage();
		}
	}

	switch (ngc_mode) {
	case NGC_MILLING:
		ngc_addr = ngc_maddr;
		break;
	case NGC_TURNING:
		ngc_addr = ngc_taddr;
		break;
	default:
		break;
	}

	/* Parse input program code into ngc_progs. */
	argv += optind;
	argc -= optind;
	for (i = 0; i < argc; i++) {
		if (ngc_parse_file(argv[i]) == -1)
			errx(1, "%s:%s", argv[i], cnc_get_error());
	}
	for (prog = TAILQ_FIRST(&ngc_progs);
	     prog != TAILQ_END(&ngc_progs);
	     prog = progNext) {
		progNext = TAILQ_NEXT(prog, progs);
		if (prog->nBlks == 0)
			ngc_prog_del(prog);
	}
	if (TAILQ_EMPTY(&ngc_progs)) {
		errx(1, "no program to execute");
	}

	/* Query the current position. */
	loadposition();

	/* Execute the main program. */
	TAILQ_FOREACH(prog, &ngc_progs, progs) {
		if (prog->name == 0)
			break;
	}
	if (prog == NULL) {
		errx(1, "program main not found\n");
	}
	verbose("main: %d blocks\n", prog->nBlks);
	for (i = 0; i < prog->nBlks; i++) {
		struct ngc_block *blk = &prog->blks[i];
		struct ngc_word *w;

		if (verbose) {
			TAILQ_FOREACH(w, &blk->words, words) {
				const struct ngc_address *addr =
				    &ngc_addr[w->type-0x41];
			
				verbose("%c", w->type);
				switch (addr->type) {
				case NGC_INTEGER:
					verbose("%d ", w->data.i);
					break;
				case NGC_SIGNED_REAL:
				case NGC_UNSIGNED_REAL:
					verbose("%f ", w->data.f);
					break;
				default:
					break;
				}
			}
			verbose("\n");
		}
		if (ngc_block_exec(prog, i) == -1)
			errx(1, "%s:%d: %s", prog->path,
			    blk->line, cnc_get_error());
	}
	return (0);
}
