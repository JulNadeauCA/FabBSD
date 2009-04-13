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

/*
 * Block processing routines.
 *
 * [1] Thomas R. Kramer, Frederick M. Proctor, and Elena Messina, The NIST
 *     RS274NGC Interpreter - Version 3, National Institute of Standards and
 *     Technology, NISTIR 6556, August 2000.
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

/*
 * XXX
 * A: Inserer blocks G0, G1, G2, ... artificiellement
 * en preprocessing.
 *
 * B: Garder flags pour G0, G1, G2, ... dans la state.
 * XXX
 */

/* Delay execution for a specified amount of time */
static int
dwell(struct ngc_block *blk, struct ngc_word *w)
{
	struct ngc_word *P;

	if ((P = ngc_getparam(blk,'P')) != NULL) {
		if (P->data.i < 0) {
			cnc_set_error("G4: P cannot be negative");
			return (-1);
		}
		sleep(P->data.i);
	} else {
		cnc_set_error("G4: missing P argument");
		return (-1);
	}
	return (0);
}

/*
 * Set spindle speed
 */
static int
spindle_set_speed(struct ngc_block *blk, struct ngc_word *w)
{
	if (w->data.i < 0) {
		cnc_set_error("S cannot be negative");
		return (-1);
	}
	ngc_spinspeed = w->data.i;
	/* TODO spindle(4) */
	return (0);
}

/*
 * Select a tool
 */
static int
tool_set(struct ngc_block *blk, struct ngc_word *w)
{
	if (w->data.i < 0) {
		cnc_set_error("T cannot be negative");
		return (-1);
	}
	/* XXX TODO: validate tool# with atc(4) */
	ngc_tool = w->data.i;
	/* TODO issue atc(4) prepare */
	return (0);
}

/* Initiate a tool change */
static int
tool_change(struct ngc_block *blk, struct ngc_word *w)
{
	/* TODO issue atc(4) change */
	return (0);
}

/* Set the cutter radius offset. */
static int
set_cutter_radius_offs(struct ngc_block *blk, struct ngc_word *w)
{
	struct ngc_word *D;

	if (w->data.i == 40) {					/* G40 */
		ngc_cromode = NGC_CUTTER_OFFS_OFF;
		return (0);
	}
	if (ngc_cromode != NGC_CUTTER_OFFS_OFF) {
		cnc_set_error("G%d: Cutter radius offset is already on",
		    w->data.i);
		return (-1);
	}
	if (ngc_planesel != NGC_PLANE_XY) {
		cnc_set_error("G%d: the XY plane is not active", w->data.i);
		return (-1);
	}
	ngc_cromode = (w->data.i == 41) ? NGC_CUTTER_OFFS_LEFT :
	                                  NGC_CUTTER_OFFS_RIGHT;

	if ((D = ngc_getparam(blk,'D')) != NULL) {
		if (D->data.i < 0) {
			cnc_set_error("G%d: D cannot be negative", w->data.i);
			return (-1);
		}
		/* XXX TODO: validate tool# with atc(4) */
	}
	return (0);
}

/* Set the tool length offset. */
static int
set_tool_length_offs(struct ngc_block *blk, struct ngc_word *w)
{
	struct ngc_word *H;
	
	switch (w->data.i) {
	case 43:
		if ((H = ngc_getparam(blk,'H')) == NULL) {
			cnc_set_error("G43: missing H parameter");
			return (-1);
		}
		if (H->data.i < 0) {
			cnc_set_error("G43: H cannot be negative");
			return (-1);
		}
		/* XXX TODO: validate tool# with atc(4) */
		ngc_tlo = H->data.i;
		return (0);
	case 49:
		ngc_tlo = 0;
		return (0);
	}
	return (0);
}

/* Rapid motion to given position */
static int
fastmove(ngc_vec_t *nPos)
{
	struct cnc_velocity Vp;
	cnc_vec_t kPos;
	int i;

	for (i = 0; i < NGC_NAXES; i++) {
		kPos.v[i] = (ngc_axis_types[i] == NGC_LINEAR) ?
		            nPos->v[i]*ngc_linscale :
			    nPos->v[i]*ngc_rotscale;
	}
	Vp.v0 = 100;				/* XXX use machine default! */
	Vp.F = (cnc_vel_t)(ngc_feedfast*ngc_feedscale);
	Vp.Amax = (cnc_vel_t)10;		/* XXX use machine default! */
	Vp.Jmax = (cnc_vel_t)10;		/* XXX use machine default! */

	return cncmove(&Vp, &kPos);
}

/* Return to home (through an optional intermediate point). */
static int
gohome(struct ngc_block *blk, struct ngc_word *w)
{
	ngc_vec_t T, Tint;
	int i, j;

	/* Move to intermediate position if specified */
	T = ngc_pos;
	if (ngc_axiswords_to_vec(blk, &Tint) > 0) {
		if (fastmove(&Tint) == -1) {
			cnc_set_error("G%d: moving to intermediate point: %s",
			    w->data.i, cnc_get_error());
			return (-1);
		}
	}

	/* Move to home position (#5161 or #5181) */
	for (i = 0, j = ((w->data.i == 28) ? 5161 : 5181);
	     i < NGC_NAXES;
	     i++) {
		T.v[i] = ngc_params[j];
	}
	return fastmove(&T);
}

/* Set coordinate system (or "work offset") origin */
static int
setcsysorigin(struct ngc_block *blk, struct ngc_word *w)
{
	struct ngc_word *P, *a;
	int i, b;

	if ((P = ngc_getparam(blk, 'P')) == NULL) {
		cnc_set_error("G10 L2: missing P number");
		return (-1);
	}
	if (P->data.i < 1 || P->data.i > 9) {
		cnc_set_error("G10 L2: invalid P number %d", P->data.i);
		return (-1);
	}
	b = ngc_csys_bases[P->data.i];
	for (i = 0; i < NGC_NAXES; i++) {
		if ((a = ngc_getparam(blk, ngc_axis_words[i])))
			ngc_params[b+i] = a->data.f;
	}
	return (0);
}

/* Set coordinate system axis offsets. */
static int
setcsysoffset(struct ngc_block *blk, struct ngc_word *w)
{
	struct ngc_word *a;
	int i;
	
	for (i = 0; i < NGC_NAXES; i++) {
		if (ngc_getparam(blk, ngc_axis_words[i]) != NULL)
			break;
	}
	if (i == NGC_NAXES) {
		cnc_set_error("G%d: missing axis words", w->data.i);
		return (-1);
	}

	switch (w->data.i) {
	case 92:				/* Set/save axis offsets */
		for (i = 0; i < NGC_NAXES; i++) {
			if ((a = ngc_getparam(blk, ngc_axis_words[i])) != NULL) {
				ngc_csys_offset[i] += ngc_pos.v[i] - a->data.f;
				ngc_params[5211+i] = ngc_csys_offset[i];
			}
		}
		break;
	case 92001:				/* Reset/save axis offsets */
		for (i = 0; i < NGC_NAXES; i++) {
			ngc_csys_offset[i] = 0.0;
			ngc_params[5211+i] = 0.0;
		}
		break;
	case 92002:				/* Reset axis offsets */
		for (i = 0; i < NGC_NAXES; i++) {
			ngc_csys_offset[i] = 0.0;
		}
		break;
	case 92003:				/* Load axis offsets */
		for (i = 0; i < NGC_NAXES; i++) {
			ngc_csys_offset[i] = ngc_params[5211+i];
		}
		break;
	}
	return (0);
}

/* Perform rapid linear motion (G0) */
static int
movelinear_fast(struct ngc_block *blk)
{
	ngc_vec_t nPos;

	/* Compute the target vector */
	nPos = ngc_pos;
	if (ngc_axiswords_to_vec(blk, &nPos) < 1) {
		cnc_set_error("G0: All axis words are omitted");
		return (-1);
	}
	return fastmove(&nPos);
}

/* Perform linear motion at feedrate (G1) */
static int
movelinear_feed(struct ngc_block *blk)
{
	struct cnc_velocity Vp;
	ngc_vec_t nPos;
	cnc_vec_t kPos;
	ngc_real_t F;
	int i;

	/* Compute the target vector. */
	nPos = ngc_pos;
	if (ngc_axiswords_to_vec(blk, &nPos) < 1) {
		cnc_set_error("G1: All axis words are omitted");
		return (-1);
	}

	/* Compute the velocity profile from the desired feedrate. */
	switch (ngc_feedmode) {
	case NGC_UNITS_PER_MIN:
		Vp.v0 = 100;			/* XXX use machine default! */
		Vp.F = (cnc_vel_t)(ngc_feedrate*ngc_feedscale);
		Vp.Amax = (cnc_vel_t)10;	/* XXX use machine default! */
		Vp.Jmax = (cnc_vel_t)10;	/* XXX use machine default! */
		F = ngc_feedrate*ngc_feedscale;
		break;
	case NGC_ONE_OVER_MIN:
		/* XXX TODO */
		cnc_set_error("G93 mode not yet implemented");
		return (-1);
	}
	for (i = 0; i < NGC_NAXES; i++) {
		kPos.v[i] = (ngc_axis_types[i] == NGC_LINEAR) ?
		            nPos.v[i]*ngc_linscale :
			    nPos.v[i]*ngc_rotscale;
	}
	if (cncmove(&Vp, &kPos) == -1) {
		cnc_set_error("G1: %s", cnc_get_error());
		return (-1);
	}
	return (0);
}

/* Perform circular or helical arc motion at feedrate (G2/G3) */
static int
movearc_feed(struct ngc_block *blk, struct ngc_word *w)
{
#if 0
	ngc_vec_t T;
	int dir = (w->data.i == 2) ? NGC_CW : NGC_CCW;

	T = ngc_pos;
	if (ngc_axiswords_to_vec(blk,&T) < 1) {
		cnc_set_error("G1: All axis words are omitted");
		return (-1);
	}
	if (ngc_move_arc(&T, ngc_feedrate) == -1) {
		return (-1);
	}
	return (0);
#endif
	cnc_set_error("unimplemented arcs");
	return (-1);
}

/*
 * Execute an instruction block. The order of processing is described on
 * p.41 of [1]. At this point, we assume that previously enabled modal
 * commands have been inserted into the block in preprocessing.
 */
int
ngc_block_exec(struct ngc_prog *prog, int iBlk)
{
	struct ngc_block *blk = &prog->blks[iBlk];
	struct ngc_word *w;
	int case18 = 0, rv;

	/* 1. Set feed rate mode */
	if (ngc_getcmd(blk,'G',94)) { ngc_feedmode = NGC_UNITS_PER_MIN; }
	else if (ngc_getcmd(blk,'G',93)) { ngc_feedmode = NGC_ONE_OVER_MIN; }

	/* 2. Set feed rate */
	if ((w = ngc_getparam(blk,'F')) != NULL) {
		ngc_feedrate =
		    (ngc_feedmode == NGC_UNITS_PER_MIN) ? ngc_units_to_mm(w->data.f) :
		    w->data.f;
	} else if (ngc_feedmode == NGC_ONE_OVER_MIN) {
		if (ngc_getcmd(blk,'G',1) ||
		    ngc_getcmd(blk,'G',2) ||
		    ngc_getcmd(blk,'G',3)) {
			cnc_set_error("G93: missing F-word");
			return (-1);
		}
	}

	/* 3. Set spindle speed */
	if ((w = ngc_getparam(blk,'S')) != NULL &&
	    spindle_set_speed(blk, w)) {
		return (-1);
	}

	/* 4. Select tool */
	if ((w = ngc_getparam(blk,'T')) != NULL &&
	    tool_set(blk, w) == -1) {
		return (-1);
	}
	/* 5. Apply tool changes */
	if ((w = ngc_getcmd(blk,'M',6)) != NULL &&
	    tool_change(blk, w) == -1) {
		return (-1);
	}

	/* 6. Apply spindle commands */
	if (ngc_getcmd(blk,'M',3)) {
		/* TODO spindle(4) start CW */
	}
	if (ngc_getcmd(blk,'M',4)) {
		/* TODO spindle(4) start CCW */
	}
	if (ngc_getcmd(blk,'M',5)) {
		/* TODO spindle(4) stop */
	}

	/* 7. Apply coolant commands */
	if (ngc_getcmd(blk,'M',7)) {
		/* TODO coolant(4) mist on */
	}
	if (ngc_getcmd(blk,'M',8)) {
		/* TODO coolant(4) flood on */
	}
	if (ngc_getcmd(blk,'M',9)) {
		/* TODO coolant(4) all off */
	}

	/* 8. Control speed/feed override switches. */
	if (ngc_getcmd(blk,'M',48)) {
		/* TODO cncpanel speed/feed override switch enable */
	}
	if (ngc_getcmd(blk,'M',49)) {
		/* TODO cncpanel speed/feed override switch disable */
	}

	/* 9. Dwell */
	if ((w = ngc_getcmd(blk,'G',4)) != NULL) {
		if (dwell(blk, w) == -1)
			return (-1);
	}
	
	/* 10. Plane selection */
	if (ngc_getcmd(blk,'G',17)) { ngc_planesel = NGC_PLANE_XY; }
	else if (ngc_getcmd(blk,'G',18)) { ngc_planesel = NGC_PLANE_ZX; }
	else if (ngc_getcmd(blk,'G',19)) { ngc_planesel = NGC_PLANE_YZ; }

	/* 11. Unit selection */
	if (ngc_getcmd(blk,'G',20)) { ngc_units = NGC_INCH; }
	else if (ngc_getcmd(blk,'G',21)) { ngc_units = NGC_METRIC; }

	/* 12. Set cutter radius offset */
	if ((w = ngc_getcmd(blk,'G',40)) != NULL ||
	    (w = ngc_getcmd(blk,'G',41)) != NULL ||
	    (w = ngc_getcmd(blk,'G',42)) != NULL) {
		if (set_cutter_radius_offs(blk, w) == -1)
			return (-1);
	}

	/* 13. Set tool length offset */
	if ((w = ngc_getcmd(blk,'G',43)) != NULL ||
	    (w = ngc_getcmd(blk,'G',49)) != NULL) {
		if (set_tool_length_offs(blk, w) == -1)
			return (-1);
	}

	/* 14. Select coordinate system (or "work offset") */
	if (ngc_getcmd(blk,'G',54)) { ngc_csys = 1; }
	else if (ngc_getcmd(blk,'G',55)) { ngc_csys = 2; }
	else if (ngc_getcmd(blk,'G',56)) { ngc_csys = 3; }
	else if (ngc_getcmd(blk,'G',57)) { ngc_csys = 4; }
	else if (ngc_getcmd(blk,'G',58)) { ngc_csys = 5; }
	else if (ngc_getcmd(blk,'G',59)) { ngc_csys = 6; }
	else if (ngc_getcmd(blk,'G',59001)) { ngc_csys = 7; }
	else if (ngc_getcmd(blk,'G',59002)) { ngc_csys = 8; }
	else if (ngc_getcmd(blk,'G',59003)) { ngc_csys = 9; }

	/* 15. Set path control mode */
	if (ngc_getcmd(blk,'G',61)) { ngc_pathctl = NGC_EXACT_PATH; }
	else if (ngc_getcmd(blk,'G',61001)) { ngc_pathctl = NGC_EXACT_STOP; }
	else if (ngc_getcmd(blk,'G',64)) { ngc_pathctl = NGC_CONT_PATH; }

	/* 16. Set distance mode */
	if (ngc_getcmd(blk,'G',90)) { ngc_distmode = NGC_ABSOLUTE; }
	else if (ngc_getcmd(blk,'G',91)) { ngc_distmode = NGC_INCREMENTAL; }

	/* 17. Set retract mode */
	if (ngc_getcmd(blk,'G',99)) { ngc_distmode = NGC_RETRACT_PREVIOUS; }
	else if (ngc_getcmd(blk,'G',98)) { ngc_distmode = NGC_RETRACT_POSITION; }

	/* 18 (case A). Return to home (G28/G30) */
	case18 = 0;
	if ((w = ngc_getcmd(blk,'G',28)) != NULL ||
	    (w = ngc_getcmd(blk,'G',30)) != NULL) {
		case18 = 1;
		if (gohome(blk, w) == -1)
			return (-1);
	}
	
	/* 18 (case B). Set coordinate system origin (G10 L2) */
	if ((w = ngc_getcmd(blk,'G',10)) != NULL) {
		struct ngc_word *L;

		if (((L = ngc_getparam(blk,'L'))) && L->data.i == 2) {
			if (case18 != 0) {
				cnc_set_error("G10 L2: conflicts with G28/G30");
				return (-1);
			}
			case18 = 2;
			if (setcsysorigin(blk, w) == -1)
				return (-1);
		}
	}
	
	/*
	 * 18 (case C). Set coordinate system offset (also known as "position
	 * register" with Fanuc controllers).
	 */
	if ((w = ngc_getcmd(blk,'G',92)) != NULL ||
	    (w = ngc_getcmd(blk,'G',92001)) != NULL ||
	    (w = ngc_getcmd(blk,'G',92002)) != NULL ||
	    (w = ngc_getcmd(blk,'G',94)) != NULL) {
		if (case18 != 0) {
			cnc_set_error("G%d: conflicts with %s", w->data.i,
			    (case18 == 1) ? "G28/G30" : "G10 L2");
			return (-1);
		}
		if (setcsysoffset(blk, w) == -1)
			return (-1);
	}

	/*
	 * 19. Perform motion.
	 */
	rv = 0;
	if (ngc_getcmd(blk,'G',0)) {
		rv = movelinear_fast(blk);
	} else if (ngc_getcmd(blk,'G',1)) {
		rv = movelinear_feed(blk);
	} else if ((w = ngc_getcmd(blk,'G',2)) ||
	           (w = ngc_getcmd(blk,'G',3))) {
		rv = movearc_feed(blk, w);
	}
	if (rv == -1)
		return (-1);

	return (0);
}
