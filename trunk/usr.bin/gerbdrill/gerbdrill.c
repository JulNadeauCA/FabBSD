/*
 * Copyright (c) 2009-2010 Julien Nadeau (vedge@hypertriton.com).
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

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <ctype.h>

#include <cnc.h>

#define MAX_TOOLS	32

int verbose = 0;			/* Verbose mode */
int dry = 0;				/* Dry run */
FILE *fp = stdin;			/* Input file */
const char *filename = "stdin";		/* Input file name */
double x = 0.0, y = 0.0;		/* Current coordinates */
int progEnd = 0;			/* Program end requested */
int nLine = 0;				/* Current line# */
int numFormat[2] = { 0,0 };		/* Input number format */
int numFormatExpl;			/* Format explicitely given */

int inHeader;				/* In Excellon header? */
char *units;				/* Unit system in use */
char *unitDefault = "mil";		/* Default unit */

enum zero_sup_mode {
	SUP_LZ,				/* Leading zero suppression */
	SUP_TZ,				/* Trailing zero suppression */
	SUP_NONE			/* Disabled */
} zeroSupMode;

enum distance_mode {
	DIST_ABSOLUTE,			/* Interpret as absolute */
	DIST_INCREMENTAL		/* Interpret as incremental */
} distanceMode;

static void
printusage(void)
{
	extern char *__progname;
	printf("Usage: %s [-dv] [-n x.y] [-S startvel] [-F feedrate] "
	       "[-A accellim] [-J jerklim] [file ...]\n", __progname);
}

/*
 * G-codes
 */

/* G04: Dwell */
static int
g_dwell(char **s)
{
	char *ep;
	const char *c;
	double d;

	for (c = *s; *c != '\0'; c++) {
		if (*c == 'X' || *c == 'P') {
			d = strtod(&c[1], &ep);
			if (ep == &c[1]) {
				cnc_set_error("Syntax error near `%s'", c);
				return (-1);
			}
			/* XXX TODO: fractional sleep? */
			sleep((int)d);
			*s = ep;
		}
	}
	return (0);
}

/* G90: Set Absolute distance mode */
static int
g_abs_mode(char **s)
{
	distanceMode = DIST_ABSOLUTE;
	return (0);
}

/* G91: Set Incremental distance mode */
static int
g_inc_mode(char **s)
{
	distanceMode = DIST_INCREMENTAL;
	return (0);
}

const struct {
	int code;
	int (*fn)(char **);
	const char *desc;
} Gcodes[] = {
	{  4,	g_dwell,	"Variable dwell (ignore)" },
	{  5,	NULL,		"Set drill mode (default)" },
	{ 81,	NULL,		"Set drill mode (default)" },
	{ 90,	g_abs_mode,	"Use absolute distances" },
	{ 91,	g_inc_mode,	"Use incremental distances" },
};
int nGcodes = sizeof(Gcodes) / sizeof(Gcodes[0]);

/*
 * M-codes
 */

/* G00: Program stop */
static int
m_stop(char **s)
{
	char key;
	printf("Program stop!\n");
	printf("Press any key to resume execution...\n");
	fread(&key, 1, 1, stdin);
	return (0);
}

/* G01: Program optional stop */
static int
m_ostop(char **s)
{
	char key;
	printf("Optional program stop!\n");
	printf("Press any key to resume execution...\n");
	fread(&key, 1, 1, stdin);
	return (0);
}

/* G02/G30: Program end */
static int
m_end(char **s)
{
	progEnd = 1;
	return (0);
}

/* M47: Operator message */
static int
m_comment(char **s)
{
	if (verbose) {
		printf("Comment: %s\n", *s);
	}
	while (**s != '\0') {
		(*s)++;
	}
	return (0);
}

/* M48: Start Excellon header */
static int
m_excellon_header(char **s)
{
	inHeader = 1;
	return (0);
}

/* M71: Set units to metric; default numerical format "###.###" */
static int
m_set_units_mm(char **s)
{
	if (!numFormatExpl && numFormat[0]==0 && numFormat[1]==0) {
		numFormat[0] = 3;
		numFormat[1] = 3;
	}
	units = "mm";
	return (0);
}

/* Set units to centimeters; default numerical format "###.###". */
static int
m_set_units_cm(char **s)
{
	if (!numFormatExpl && numFormat[0]==0 && numFormat[1]==0) {
		numFormat[0] = 3;
		numFormat[1] = 3;
	}
	units = "cm";
	return (0);
}

/* Set units to mil; default numerical format "##.####". */
static int
m_set_units_mil(char **s)
{
	if (!numFormatExpl && numFormat[0]==0 && numFormat[1]==0) {
		numFormat[0] = 2;
		numFormat[1] = 4;
	}
	units = "mil";
	return (0);
}

/* Set units to inches; default numerical format "##.####". */
static int
m_set_units_in(char **s)
{
	if (!numFormatExpl && numFormat[0]==0 && numFormat[1]==0) {
		numFormat[0] = 2;
		numFormat[1] = 4;
	}
	units = "in";
	return (0);
}

/* Set leading zero format */
static int
m_set_LZ(char **s)
{
	zeroSupMode = SUP_LZ;
	return (0);
}

/* Set trailing zero format */
static int
m_set_TZ(char **s)
{
	zeroSupMode = SUP_TZ;
	return (0);
}

/*
 * M-codes
 */
const struct {
	int code;
	int (*fn)(char **);
	const char *desc;
} Mcodes[] = {
	{ 00,	m_stop,			"Compulsory program stop" },
	{ 01,	m_ostop,		"Optional program stop" },
	{ 02,	m_end,			"Program end" },
	{ 30,	m_end,			"Program end" },
	{ 47,	m_comment,		"Operator message CRT display" },
	{ 48,	m_excellon_header,	"Start Excellon header" },
	{ 71,	m_set_units_mm,		"Set units to mm" },
	{ 72,	m_set_units_in,		"Set units to inches" },
};
int nMcodes = sizeof(Mcodes) / sizeof(Mcodes[0]);

/*
 * Header definitions
 */
const struct {
	const char *code;
	int (*fn)(char **);
	const char *desc;
} Hwords[] = {
	{ "MIL",	m_set_units_mil,	"Set units to mil" },
	{ "MM",		m_set_units_mm,		"Set units to mm" },
	{ "METRIC",	m_set_units_mm,		"Set units to mm" },
	{ "CM",		m_set_units_cm,		"Set units to cm" },
	{ "INCH",	m_set_units_in,		"Set units to inches" },
	{ "LZ",		m_set_LZ,		"Leading zero format" },
	{ "TZ",		m_set_TZ,		"Trailing zero format" },
};
int nHwords = sizeof(Hwords) / sizeof(Hwords[0]);

/* Initialize a program */
static void
prog_init(void)
{
	if (!numFormatExpl) {
		numFormat[0] = 0;
		numFormat[1] = 0;
	}
	nLine = 0;
	inHeader = 0;
	units = NULL;
	zeroSupMode = SUP_NONE;
	distanceMode = DIST_ABSOLUTE;
	progEnd = 0;
}

static void
prog_begin_block(void)
{
	if (inHeader) {
		inHeader = 0;
		if (units == NULL) {
			units = unitDefault;
			if (verbose)
				printf("No units specified in header; "
				       "defaulting to %s\n", units);
		}
		if (verbose && !numFormatExpl &&
		    numFormat[0] == 0 && numFormat[1] == 0) {
			printf("No -n format specified and could not "
			       "auto-detect; expecting decimal points.\n");
		}
	}
}

/* Parse a distance value */
static int
parse_distance(char *s, char **ep, double *rv)
{
	char num[26], sign = '+';
	char *c, intPart[12], fracPart[12];
	int i;

	if (numFormat[0] != 0 && numFormat[1] != 0) {
		if (*s == '-' || *s == '+') {			/* Sign */
			sign = *s;
			s++;
		}
		for (i = 0, c = &s[0];				/* Integer */
		     i < numFormat[0] && *c != '\0';
		     i++, c++) {
			intPart[i] = *c;
		}
		intPart[i] = '\0';
		s = c;
		for (i = 0, c = &s[0];				/* Fractional */
		     i < numFormat[1] && *c != '\0';
		     i++, c++) {
			fracPart[i] = *c;
		}
		fracPart[i] = '\0';
		*ep = c;

		/* Recombine real number */
		num[0] = sign;
		num[1] = '\0';
		strlcat(num, intPart, sizeof(num));
		strlcat(num, ".", sizeof(num));
		strlcat(num, fracPart, sizeof(num));
		*rv = strtod(num, NULL);
	} else {
		*rv = strtod(num, ep);
	}
	return (0);
}

/* Process a program line */
static int
prog_exec_line(char *s)
{
	double x = 0.0, y = 0.0;
	char *ep;
	int moveAndDrill = 0;
	int i, code, which;

	if (s[0] == '%' && s[1] == '\0') {
		prog_begin_block();
	}
	if (*s == '/') {			/* Optional delete (ignore) */
		s++;
		while (isdigit(*s))
			s++;
	}
	while (*s != '\0') {
		switch (*s) {
		case ' ':
		case ',':
			s++;
			break;
		case 'N':				/* Block number */
			(void)strtoul(&s[1], &ep, 10);
			if (ep == &s[1]) { goto syntax; }
			s = ep;
			break;
		case 'S':				/* Speed/feed */
		case 'F':
			/* XXX TODO */
			(void)strtoul(&s[1], &ep, 10);
			if (ep == &s[1]) { goto syntax; }
			s = ep;
			break;
		case 'M':					/* M-code */
			code = (int)strtoul(&s[1], &ep, 10);
			if (ep == &s[1]) { goto syntax; }

			for (i = 0; i < nMcodes; i++) {
				if (Mcodes[i].code == code)
					break;
			}
			if (i == nMcodes) {
				cnc_set_error("M%d is unimplemented", code);
				return (-1);
			}
			if (verbose) {
				printf("[M%d (%s)]\n", code, Mcodes[i].desc);
			}
			s = ep;
			if (Mcodes[i].fn != NULL) {
				if (Mcodes[i].fn(&s) == -1)
					return (-1);
				if (progEnd)
					return (1);
			}
			break;
		case 'G':					/* G-code */
			code = (int)strtoul(&s[1], &ep, 10);
			if (ep == &s[1]) { goto syntax; }
			for (i = 0; i < nGcodes; i++) {
				if (Gcodes[i].code == code)
					break;
			}
			if (i == nGcodes) {
				cnc_set_error("G%d is unimplemented", code);
				return (-1);
			}
			if (verbose) {
				printf("[G%d (%s)]\n", code, Gcodes[i].desc);
			}
			s = ep;
			if (Gcodes[i].fn != NULL) {
				if (Gcodes[i].fn(&s) == -1)
					return (-1);
				if (progEnd)
					return (1);
			}
			break;
		case 'T':					/* Tool */
			which = (int)strtoul(&s[1], &ep, 10);
			if (ep == &s[1]) { goto syntax; }
			s = ep;
			if (*s == 'C') {	/* Define circular aperture */
				x = strtod(&s[1], &ep);
				if (ep == &s[1]) { goto syntax; }
				printf("Tool %d: C, dia=%f\n", which, x);
				s = ep;
			} else if (*s == '\0') {
				printf("Select tool: %d\n", which);
			} else {
				cnc_set_error("Non-circular aperture!");
				return (-1);
			}
			/* XXX TODO: atc(4) interface */
			break;
		case 'X':					/* X-coord */
			if (parse_distance(&s[1], &ep, &x) == -1 ||
			    ep == &s[1]) {
				goto syntax_coord;
			}
			s = ep;
			moveAndDrill = 1;
			break;
		case 'Y':					/* Y-coord */
			if (parse_distance(&s[1], &ep, &y) == -1 ||
			    ep == &s[1]) {
				goto syntax_coord;
			}
			s = ep;
			moveAndDrill = 1;
			break;
		default:
			if (inHeader) {
				char *tok, *pToks = s;

				while ((tok = strsep(&pToks, ",")) != NULL) {
					for (i = 0; i < nHwords; i++) {
						if (strcasecmp(Hwords[i].code,
						    tok) == 0)
							break;
					}
					if (i == nHwords) {
						goto syntax;
					}
					if (Hwords[i].fn != NULL &&
					    Hwords[i].fn(&s) == -1)
						return (-1);
				}
				return (0);
			}
			goto syntax;
		}
	}
	if (moveAndDrill) {
		struct cnc_velocity vel = cnc_vel_default;
		struct cnc_velocity velFeed = cnc_vel_default;

		cnc_vec_t v;

		v.v[0] = (int64_t)(x*3000.0);
		v.v[1] = (int64_t)(y*3000.0);
		v.v[2] = 0;
		printf("Drill at %d,%d (xy=%f,%f)\n",
		    (int)v.v[0], (int)v.v[1],
		    x, y);
		if (cncmove(&vel, &v) != 0)
			errx(1, "cncmove failed");

		velFeed.F = 1000;
		v.v[2] = -6000;
		if (cncmove(&velFeed, &v) != 0)
			errx(1, "cncmove failed");
	
		v.v[2] = 0;
		if (cncmove(&vel, &v) != 0)
			errx(1, "cncmove failed");
	}
	return (0);
syntax_coord:
	cnc_set_error("Syntax error near `%s' (make sure you have correct -n format)", s);
	return (-1);
syntax:
	cnc_set_error("Syntax error near `%s'", s);
	return (-1);
}

int
main(int argc, char *argv[])
{
	struct cnc_velocity vel;
	int i, ch, rv;
	char *s, *ep;

	if (cnc_init() == -1) {
		errx(1, "%s", cnc_get_error());
	}
	atexit(cnc_destroy);
	vel = cnc_vel_default;

	while ((ch = getopt(argc, argv, "dvn:S:F:A:J:?h")) != -1) {
		switch (ch) {
		case 'd':
			dry = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'n':
			s = optarg;
			numFormat[0] = strtoul(s, &ep, 10);
			if (ep == s || *ep != '.' || ep[1] == '\0') {
				errx(1, "Illegal -n format");
			}
			s = &ep[1];
			numFormat[1] = strtoul(s, &ep, 10);
			if (*ep != '\0') {
				errx(1, "Illegal -n format (%s)", optarg);
			}
			if (numFormat[0] > 11 ||
			    numFormat[1] > 11) {
				errx(1, "Illegal -n format");
			}
			if (verbose) {
				printf("Using format: %d.%d\n",
				    numFormat[0], numFormat[1]);
			}
			numFormatExpl = 1;
			break;
		case 'S':
			cnc_vel_parse(&vel.v0, optarg);
			break;
		case 'F':
			cnc_vel_parse(&vel.F, optarg);
			break;
		case 'A':
			cnc_vel_parse(&vel.Amax, optarg);
			break;
		case 'J':
			cnc_vel_parse(&vel.Jmax, optarg);
			break;
		default:
			printusage();
			return (1);
		}
	}
	argv += optind;
	argc -= optind;
	if (argc == 0) {
		printusage();
		return (1);
	}
	for (i = 0; i < argc; i++) {
		char *s, *cStart, *pStart, *pEnd;
		char *lbuf;
		size_t size;

		if (!strcmp(argv[i], "-")) {
			fp = stdin;
			filename = "stdin";
		} else {
			if ((fp = fopen(argv[i], "r")) == NULL) {
				err(1, "%s", argv[i]);
			}
			filename = argv[i];
		}
		if (verbose) {
			printf("Running: %s\n", filename);
		}
		prog_init();
		while ((s = fgetln(fp, &size)) != NULL) {
			pEnd = &s[size-1];
			if (*pEnd == '\n') {
				if (pEnd > s && pEnd[-1] == '\r') {
					pEnd[-1] = '\0';
				}
				*pEnd = '\0';
				lbuf = NULL;
			} else {
				if ((lbuf = malloc(size+1)) == NULL) {
					err(1, NULL);
				}
				memcpy(lbuf, s, size);
				lbuf[size] = '\0';
				s = lbuf;
			}
			while (isspace(*s) && s < pEnd) {
				s++;
			}
			pStart = s;
			
			for (cStart = s; *cStart != '\0'; cStart++) {
				if (*cStart == ';') {
					*cStart = '\0';
					break;
				} else if (*cStart == '(') {
					/* XXX TODO multiline comments */
					*cStart = '\0';
					break;
				}
			}
			if (*s == '\0' ||
			    (s[0] == '%' && s[1] == '\0')) {
				nLine++;
				continue;
			}
			if ((rv = prog_exec_line(s)) == -1) {
				goto fail;
			} else if (rv == 1) {
				break;
			}
			free(lbuf);
			nLine++;
		}
		fclose(fp);
	}
	return (0);
fail:
	errx(1, "%s:%d: %s", filename, nLine, cnc_get_error());
	return (1);
}

