/*	$FabBSD$	*/
/*
 * Copyright (c) 2010 Hypertriton, Inc. <http://hypertriton.com/>
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

#include <cnc.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

const char *cnc_tool_strings[] = {
	"none",
	"drill",
	"holesaw",
	"endmill",
	"slitsaw",
	"facemill",
	"gearcutter"
};
const char *cnc_endmill_nose_strings[] = {
	"flat",
	"rounded",
	"tapered",
	"ball",
	"chamfer",
	"dovetail",
	"t-slot",
	"corner-rounding",
	"convex-radius",
	"concave-radius"
};

#define FIELD_SEP "|:;,/ "

/*
 * Initialize tool from a drill specification string:
 * diameter:[length]:[point-angle]
 */
static int
cnc_parse_drill(char *s, struct cnc_tool *t)
{
	char *tok, *ep;
	int wire;

	/* Parse the drill diameter */
	if ((tok = strsep(&s, FIELD_SEP)) == NULL) {
		cnc_set_error("Missing drill diameter");
		return (-1);
	}
	if (isalpha(tok[0]) && isupper(tok[0])) {
		t->data.drill.dia = cnc_drill_letter[(*tok)-'A'];
	} else if ((tok[0] == '#' || tolower(tok[0]) == 'w') &&
	           tok[1] != '\0') {
		wire = (int)strtoul(&tok[1], &ep, 10);
		if (wire < 0 || wire > 80) {
			cnc_set_error("No such wire drill number: %d", wire);
			return (-1);
		}
		t->data.drill.dia = cnc_drill_wire[wire];
	} else {
		if (cnc_dist_parse(&t->data.drill.dia, tok) == -1) {
			cnc_set_error("Drill diameter: %s", cnc_get_error());
			return (-1);
		}
	}
	
	/* Parse the drill length */
	if ((tok = strsep(&s, FIELD_SEP)) != NULL &&
	    tok[0] != '\0') {
		if (cnc_dist_parse(&t->data.drill.len, tok) == -1) {
			cnc_set_error("Drill length: %s", cnc_get_error());
			return (-1);
		}
	}
	
	/* Parse the drill point angle */
	if ((tok = strsep(&s, FIELD_SEP)) != NULL &&
	    tok[0] != '\0') {
		if (cnc_angle_parse(&t->data.drill.ptAngle, tok) == -1) {
			cnc_set_error("Drill point angle: %s", cnc_get_error());
			return (-1);
		}
	}
	return (0);
}
static void
cnc_print_drill(const struct cnc_tool *t)
{
	printf("%f dia", t->data.drill.dia);
	if (t->data.drill.len != 0)
		printf(", %f len", t->data.drill.len);
	if (t->data.drill.ptAngle != 0)
		printf(", %d deg", t->data.drill.ptAngle);
}

/*
 * Initialize tool from a hole saw specification string:
 * diameter:[length]:[point-angle]
 */
static int
cnc_parse_holesaw(char *s, struct cnc_tool *t)
{
	char *tID, *tOD, *ep;
	int wire;

	/* Parse the hole saw ID and OD */
	if ((tID = strsep(&s, FIELD_SEP)) == NULL ||
	    (tOD = strsep(&s, FIELD_SEP)) == NULL) {
		cnc_set_error("Missing ID/OD");
		return (-1);
	}
	if (cnc_dist_parse(&t->data.holesaw.id, tID) == -1) {
		cnc_set_error("Holesaw ID: %s", cnc_get_error());
		return (-1);
	}
	if (cnc_dist_parse(&t->data.holesaw.od, tOD) == -1) {
		cnc_set_error("Holesaw OD: %s", cnc_get_error());
		return (-1);
	}
	return (0);
}
static void
cnc_print_holesaw(const struct cnc_tool *t)
{
	printf("%f ID, %f OD", t->data.holesaw.id, t->data.holesaw.od);
}


const struct {
	int  (*parseFn)(char *s, struct cnc_tool *);
	void (*printFn)(const struct cnc_tool *);
} cnc_tool_fns[] = {
	{ NULL,			NULL },
	{ cnc_parse_drill,	cnc_print_drill },
	{ cnc_parse_holesaw,	cnc_print_holesaw }
};

/* Parse a tool specification string. */
int
cnc_tool_parse(const char *s, struct cnc_tool *t)
{
	char *sDup;
	int i;

	for (i = 0; i < CNC_TOOL_LAST; i++) {
		if (strncasecmp(s, cnc_tool_strings[i],
		    strlen(cnc_tool_strings[i])) == 0)
			break;
	}
	if (i == CNC_TOOL_LAST) {
		cnc_set_error("No such tool: `%s'", s);
		return (-1);
	}
	if ((sDup = strdup(s)) == NULL) {
		cnc_set_error("Out of memory");
		return (-1);
	}
	if (cnc_tool_fns[i].parseFn(sDup, t) == -1) {
		free(sDup);
		return (-1);
	}
	free(sDup);
	return (0);
}

/* Print tool information */
void
cnc_tool_print(const struct cnc_tool *t)
{
	fputs( "  [", stdout);
	if (t->atc != -1) {
		printf("atc%d:", t->atc);
	}
	fputs(cnc_tool_strings[t->type], stdout);
	if (t->type == CNC_TOOL_ENDMILL) {
		fputc('/', stdout);
		fputs(cnc_endmill_nose_strings[t->data.endmill.nose], stdout);
	}
	fputs("] ", stdout);

	switch (t->type) {
	case CNC_TOOL_DRILL:
		printf(" dia=%f", t->data.drill.dia);
		if (t->data.drill.len != 0)
			printf(" len=%f", t->data.drill.len);
		if (t->data.drill.ptAngle != 0)
			printf(" point=%d", t->data.drill.ptAngle);
		break;
	case CNC_TOOL_HOLESAW:
		printf(" ID=%f", t->data.holesaw.id);
		printf(" OD=%f", t->data.holesaw.od);
		if (t->data.holesaw.len != 0)
			printf(" len=%f", t->data.holesaw.len);
		break;
	case CNC_TOOL_ENDMILL:
		printf(" %s=%f",
		    (t->data.endmill.nose == CNC_ENDMILL_TAPERED) ? "tip-dia":"dia",
		    t->data.endmill.dia);
		printf(" %s=%f",
		    (t->data.endmill.nose == CNC_ENDMILL_TSLOT) ? "face-wd":"len",
		    t->data.endmill.len);
		switch (t->data.endmill.nose) {
		case CNC_ENDMILL_CHAMFER:
		case CNC_ENDMILL_DOVETAIL:
			printf(" angle=%d", t->data.endmill.angle);
			break;
		case CNC_ENDMILL_ROUNDED:
		case CNC_ENDMILL_CORNER_ROUNDING:
		case CNC_ENDMILL_CONVEX_RADIUS:
		case CNC_ENDMILL_CONCAVE_RADIUS:
			printf(" radius=%f", t->data.endmill.r);
			break;
		}
		break;
	default:
		break;
	}
	printf("\n");
}
