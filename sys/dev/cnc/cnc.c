/*	$FabBSD$	*/
/*
 * Copyright (c) 2007-2011 Hypertriton, Inc. <http://www.hypertriton.com/>
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
 * Real-time control interface for CNC machinery and robots.
 */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/pool.h>
#include <sys/conf.h>
#include <sys/lock.h>
#include <sys/mount.h>

#include <sys/syscallargs.h>

#include <sys/gpio.h>
#include <sys/cnc.h>

#include <dev/gpio/gpiovar.h>
#include <machine/bus.h>

#include "cnc_devicevar.h"
#include "cnc_servovar.h"
#include "cnc_spindlevar.h"
#include "cnc_estopvar.h"
#include "cnc_encodervar.h"
#include "cnc_mpgvar.h"
#include "cnc_statledvar.h"
#include "cnc_lcdvar.h"
#include "cnc_spotweldervar.h"

#include "cncvar.h"
#include "cnc_capturevar.h"

#include "servo.h"
#include "spindle.h"
#include "estop.h"
#include "encoder.h"
#include "mpg.h"
#include "cnclcd.h"
#include "cncstatled.h"
#include "spotwelder.h"

/* For cnc_pos_t (uint64), /STEPDIV */
#define STEPLEN 20336
#define STEPMAX	(INT64_MAX-1)
#define MAXSTEPS (STEPMAX/STEPLEN)

/* #define CNC_DEBOUNCE */

struct cnc_kinlimits cnc_kinlimits = {
	20000,				/* Max steps/sec */
	200000,				/* Max steps/ms^2 */
	8000000				/* Max steps/ms^3 */
};

const char *cnc_axis_names[] = {
	"X", "Y", "Z",			/* Primary axes */
	"U", "V", "W",			/* Secondary axes */
	"I", "J", "K",			/* Arc center vectors */
	"A", "B", "C"			/* Rotary axes */
};

int cnc_opened;
struct cnc_vector cnc_pos;
struct cnc_timings cnc_timings;
struct cnc_stats cnc_stats;

struct servo_softc      *cnc_servos[CNC_MAX_SERVOS];
struct spindle_softc    *cnc_spindles[CNC_MAX_SPINDLES];
struct atc_softc        *cnc_atcs[CNC_MAX_ATCS];
struct estop_softc      *cnc_estops[CNC_MAX_ESTOPS];
struct encoder_softc    *cnc_encoders[CNC_MAX_ENCODERS];
struct mpg_softc        *cnc_mpgs[CNC_MAX_MPGS];
struct cnclcd_softc     *cnc_lcds[CNC_MAX_LCDS];
struct cncstatled_softc *cnc_status_led = NULL;
struct spotwelder_softc *cnc_spotwelders[CNC_MAX_SPOTWELDERS];

int cnc_debug = 0;
int cnc_simulate = 0;
int cnc_capture = 0;
int cnc_nservos = 0;
int cnc_nspindles = 0;
int cnc_natcs = 0;
int cnc_nestops = 0;
int cnc_nencoders = 0;
int cnc_nmpgs = 0;
int cnc_nlcds = 0;
int cnc_nspotwelders = 0;

void
cncattach(int num)
{
	int i;

	if (num > 1)
		return;

	cnc_opened = 0;

	for (i = 0; i < CNC_NAXES; i++)
		cnc_pos.v[i] = 0;
	
	cnc_timings.hz = 0;
	cnc_stats.peak_velocity = 0;
	cnc_stats.estops = 0;
}

int
cncdetach(struct device *self, int flags)
{
#if NCNCLCD > 0
	int i;
	for (i = 0; i < cnc_nlcds; i++)
		cnclcd_close(cnc_lcds[i]);
#endif
#ifdef MOTIONCAPTURE
	if (cnc_capture)
		cnc_capture_close();
#endif
	return (0);
}

int
cncactivate(struct device *self, enum devact a)
{
	return (0);
}

int
cncopen(dev_t dev, int flag, int mode, struct proc *p)
{
	if (cnc_opened) {
		return (EBUSY);
	}
	cnc_opened = 1;
	return (0);
}

int
cncclose(dev_t dev, int flag, int mode, struct proc *p)
{
#ifdef MOTIONCAPTURE
	if (cnc_capture)
		cnc_capture_close();
#endif
	cnc_opened = 0;
	return (0);
}

int
cncread(dev_t dev, struct uio *uio, int ioflag)
{
#ifdef MOTIONCAPTURE
	return cnc_capture_read(dev, uio, ioflag);
#else
	return (ENODEV);
#endif
}

int
cncwrite(dev_t dev, struct uio *uio, int ioflag)
{
	return (ENODEV);
#if 0
	const char *cause;
	struct cnc_insn insn, *iNew;
	u_int ip = 0;

	while (uio->uio_resid > 0) {
		int rv;

		rv = uiomove(&insn, sizeof(struct cnc_insn), uio);
		if (rv != 0) {
			printf("cncwrite: uiomove: %d\n", rv);
			return (EIO);
		}
		if (insn.i_type < 0 || insn.i_type >= CNC_LAST_INSN) {
			printf("cncwrite: bad insn %d\n", insn.i_type);
			return (ENODEV);
		}
		if ((iNew = pool_get(&cnc_insnpl, 0)) == NULL) {
			cause = "out of memory for instruction";
			goto fail;
		}
		memcpy(iNew, &insn, sizeof(struct cnc_insn));
		TAILQ_INSERT_TAIL(&cnc_prog, iNew, prog);
		ip++;
	}
	return (0);
#endif
}

void
cnc_message(const char *msg)
{
#if NCNCLCD > 0
	int i;
	for (i = 0; i < cnc_nlcds; i++) {
		struct cnclcd_softc *lcd = cnc_lcds[i];

		if (!lcd->sc_open &&
		    cnclcd_open(cnc_lcds[i], 9600, 7, CNCLCD_PEVEN, 1) == -1) {
			printf("%s: cannot open\n", ((struct device *)lcd)->dv_xname);
			continue;
		}
		cnclcd_puts(lcd, msg);
		cnclcd_putc(lcd, '\n');
	}
#endif
}

/* Calibrate the delay loop for 1Hz reference. */
cnc_utime_t
cnc_calibrate_hz(void)
{
	const int maxIters = 100;
	const cnc_utime_t tTgt = 1e6; /* 1s */
	struct timeval tv1, tv2;
	cnc_utime_t t, r, i;
	cnc_time_t d;
	int s, j;

	/* Start from an arbitrary value. XXX TODO use cpu info */
	r = 380000000U;

	s = splhigh();
	printf("cnc: calibrating delay loop for 1Hz (%d iters max)...",
	    maxIters);
	for (j = 0; j < maxIters; j++) {
		microtime(&tv1);
		for (i = 0; i < r; i++)
			;;
		microtime(&tv2);

		t = (tv2.tv_sec - tv1.tv_sec)*1e6 +
		    (tv2.tv_usec - tv1.tv_usec);

		splx(s);
		preempt(NULL);
		s = splhigh();
		printf(" %ld", tTgt-t);
		if (t < tTgt) {
			d = tTgt - t;
			if (d < 10) { break; }
			r += d*4;
		} else if (t > tTgt) {
			d = t - tTgt;
			if (d < 10) { break; }
			r -= d*4;
		} else {
			break;
		}
	}
	splx(s);
	printf("...using %llu\n", r);
	return (r);
}

int
cncioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	cnc_vec_t *pos;
	int i;

	switch (cmd) {
	case CNC_GETPOS:
		pos = (cnc_vec_t *)data;
		for (i = 0; i < CNC_NAXES; i++) {
			pos->v[i] = cnc_pos.v[i];
		}
		return (0);
	case CNC_SETPOS:
		pos = (cnc_vec_t *)data;
		for (i = 0; i < CNC_NAXES; i++) {
			if (cnc_debug) {
				printf("cnc: SETPOS axis#%d: %lu -> %lu\n", i,
				    cnc_pos.v[i], (u_long)pos->v[i]);
			}
			cnc_pos.v[i] = pos->v[i];
		}
		return (0);
	case CNC_GETDEVICEINFO:
		{
			struct cnc_device_info *di =
			    (struct cnc_device_info *)data;

			di->nservos = cnc_nservos;
			di->nspindles = cnc_nspindles;
			di->natcs = cnc_natcs;
			di->nestops = cnc_nestops;
			di->nencoders = cnc_nencoders;
			di->nmpgs = cnc_nmpgs;
		}
		return (0);
	case CNC_GETKINLIMITS:
		bcopy(&cnc_kinlimits, (void *)data, sizeof(struct cnc_kinlimits));
		return (0);
	case CNC_SETKINLIMITS:
		bcopy((const void *)data, &cnc_kinlimits, sizeof(struct cnc_kinlimits));
		return (0);
	case CNC_GETTIMINGS:
		bcopy(&cnc_timings, (void *)data, sizeof(struct cnc_timings));
		return (0);
	case CNC_SETTIMINGS:
		bcopy((const void *)data, &cnc_timings, sizeof(struct cnc_timings));
		return (0);
	case CNC_CALTIMINGS:
		if (cnc_timings.hz == 0) {
			cnc_timings.hz = cnc_calibrate_hz();
		}
		bcopy(&cnc_timings, (void *)data, sizeof(struct cnc_timings));
		return (0);
	case CNC_SETCAPTUREMODE:
#ifdef MOTIONCAPTURE
		if (*(int *)data) {
			cnc_capture_open();
		} else {
			cnc_capture_close();
		}
		return (0);
#else
		return (ENODEV);
#endif
	case CNC_GETSTATS:
		bcopy(&cnc_stats, (void *)data, sizeof(struct cnc_stats));
		return (0);
	case CNC_CLRSTATS:
		bzero(&cnc_stats, sizeof(struct cnc_stats));
		return (0);
	case CNC_GETNTOOLS:
		*(int *)data = cnc_ntools;
		return (0);
	case CNC_GETTOOLINFO:
		{
			struct cnc_tool *tool = (struct cnc_tool *)data;
			int idx = tool->idx;

			if (idx < 0 || idx >= cnc_ntools) {
				return (EINVAL);
			}
			bcopy(&cnc_tools[idx], (void *)data,
			    sizeof(struct cnc_tool));
		}
		return (0);
	case CNC_SETTOOLINFO:
		{
			struct cnc_tool *tool = (struct cnc_tool *)data;
			int idx = tool->idx;

			if (idx < 0 || idx >= cnc_ntools) {
				return (EINVAL);
			}
			bcopy((void *)data, &cnc_tools[idx],
			    sizeof(struct cnc_tool));
		}
		return (0);
	default:
		break;
	}
	return (ENODEV);
}

/* Return 1 if one of the registered e-stops is raised. */
int
cnc_estop_raised(void)
{
#if NESTOP > 0
	int i;

	for (i = 0; i < cnc_nestops; i++) {
		struct estop_softc *estop = cnc_estops[i];

		if (!estop_get_state(estop)) {
			continue;
		}
		if (estop->sc_open) {
			cnc_stats.estops++;
			estop->sc_queued = 1;
			continue;
		}
		if (estop->sc_flags & ESTOP_DEBOUNCE) {
			if (estop_debounce(estop)) {
				cnc_stats.estops++;
				return (1);
			}
		} else {
			cnc_stats.estops++;
			return (1);
		}
	}
#endif /* NESTOP > 0 */
	return (0);
}

#if NSERVO > 0

/* Move an axis one increment in the specified direction. */
void
cnc_inc_axis(enum cnc_axis axis, int dir)
{
	servo_step(cnc_servos[axis], dir);
	cnc_pos.v[axis] += dir;
}

void
cnc_clip_velocity(const struct cnc_quintic_profile *Q, cnc_real_t *vel)
{
	int clipped = 0;

	if (*vel > (cnc_real_t)cnc_kinlimits.Fmax) {
		*vel = (cnc_real_t)cnc_kinlimits.Fmax;
		clipped = 1;
	}
	if (*vel < Q->v0) {
		*vel = Q->v0;
		clipped = 1;
	}
#if NCNCSTATLED > 0
	cncstatled_set(clipped);
#endif
	if (*vel > cnc_stats.peak_velocity)
		cnc_stats.peak_velocity = *vel;
}

/*
 * Move a group of servo/stepper driven axes in a coordinated fashion at
 * the specified velocity, subject to our kinematic limits (Amax/Jmax/F).
 *
 * TODO Implement simultaneous acceleration/deceleration of axes at the
 * cost of contour errors, for specialized applications.
 */
int
sys_cncmove(struct proc *p, void *v, register_t *retval)
{
	struct sys_cncmove_args /* {
		syscallarg(const struct cnc_velocity *) vel;
		syscallarg(const cnc_vec_t *) tgt;
	} */ *uap = v;
	const struct cnc_velocity *Vp = SCARG(uap,vel);
	const cnc_vec_t *v2 = SCARG(uap,tgt);
	cnc_real_t t, dt, L;
	cnc_vec_t d, v1=cnc_pos;
	cnc_pos_t inc, incMin, vMajor=0;
	struct cnc_quintic_profile Q;
	int dir[CNC_NAXES], s, i, nonzero=0, axisMajor=0;
	
	if (cnc_calibrated())
		return (ENXIO);

	if (Vp->F == 0.0 || Vp->Amax == 0.0 || Vp->Jmax == 0.0)
		return (EINVAL);

	if (cnc_debug) {
		printf("cncmove: v0=%lus/s, F=%lus/s Amax=%lus/us^2 "
		       "Jmax=%lus/us^3\n", Vp->v0, Vp->F, Vp->Amax, Vp->Jmax);
	}

	/*
	 * Compute the difference from the current to the new position,
	 * set axisMajor to the axis with the greatest displacement.
	 */
	for (i = 0; i < CNC_NAXES; i++) {
		d.v[i] = abs(v2->v[i] - v1.v[i]);
		if (d.v[i] > vMajor) {
			vMajor = d.v[i];
			axisMajor = i;
		}
		dir[i] = (v2->v[i] > v1.v[i]) ? 1 : -1;
		if (d.v[i] != 0) { nonzero++; }
	}
	if (!nonzero)
		return (0);		/* Already at target position */

	/*
	 * Rescale the d[] values of the other axes to STEPLEN over the
	 * displacement of axisMajor.
	 */
	for (i = 0; i < CNC_NAXES; i++) {
		if (i != axisMajor)
			d.v[i] = d.v[i]*STEPLEN/d.v[axisMajor];
	}

	/* Compute the time constants for our quintic velocity profile. */
	L = (cnc_real_t)d.v[axisMajor];
	if (cnc_quintic_init(&Q, L,
	    (cnc_real_t)Vp->v0,			/* in steps/s */
	    (cnc_real_t)Vp->F,			/* in steps/s */
	    ((cnc_real_t)Vp->Amax)*1000.0,	/* ms -> us */
	    ((cnc_real_t)Vp->Jmax)*1000.0)	/* ms -> us */
	    == -1) {
		return (EINVAL);
	}
	dt = (Q.Ta+Q.To)/L;

	if (cnc_debug) {
		printf("cnc: Linear Move: L=%s ", cnc_fmt_real(L));
		printf("dt=%s ", cnc_fmt_real(dt));
		printf("F=%s ", cnc_fmt_real(Q.F));
		printf("v0=%s ", cnc_fmt_real(Q.v0));
		printf("v1=%s ", cnc_fmt_real(Q.v1));
		printf("v2=%s ", cnc_fmt_real(Q.v2));
		printf("Aref=%s ", cnc_fmt_real(Q.Aref));
		printf("[Ts=%s", cnc_fmt_real(Q.Ts));
		printf(" Ta=%s", cnc_fmt_real(Q.Ta));
		printf(" To=%s]\n", cnc_fmt_real(Q.To));
	}

	/*
	 * Enter the signal generation loop. We iterate over the displacement
	 * of the major axis.
	 */
	if (!cnc_capture) { s = splhigh(); }
	for (inc=0, t=0.0;
	     inc < d.v[axisMajor] && t < MAXSTEPS;
	     inc++, t+=dt) {
		cnc_real_t vel;
		cnc_utime_t delay, j;
#ifdef MOTIONCAPTURE
		struct cnc_capture_frame cf =
		    CNC_CAPTURE_FRAME_INITIALIZER(t);
#endif
		if (cnc_estop_raised())
			goto stop;
#if NENCODER > 0
		for (i = 0; i < cnc_nencoders; i++)
			encoder_update(cnc_encoders[i]);
#endif
		/* Increment the major axis once per iteration. */
		cnc_inc_axis(axisMajor, dir[axisMajor]);
		CNC_CAPTURE_STEP(&cf, axisMajor, dir[axisMajor]);

		/* Increment the other axes by their scaled intervals. */
		for (i = 0; i < CNC_NAXES; i++) {
			if (i != axisMajor) {
				incMin = (dir[i]*d.v[i]*t)/STEPLEN;
				if (v1.v[i]+incMin != cnc_pos.v[i]) {
					cnc_inc_axis(i, dir[i]);
					CNC_CAPTURE_STEP(&cf, i, dir[i]);
				}
			}
		}
		CNC_CAPTURE_FRAME(&cf);

		/*
		 * Evaluate optimal velocity at t in steps/second, and
		 * enter the corresponding delay loop.
		 */
		vel = cnc_quintic_step(&Q, t);
/*		cnc_clip_velocity(&Q, &vel); */

		delay = (cnc_utime_t)(cnc_timings.hz/vel);
		for (j = 0; j < delay; j++)
			;;
	}
	if (!cnc_capture) { splx(s); }
	return (0);
stop:
	if (!cnc_capture) { splx(s); }
	if (cnc_debug) {
		printf("cnc: emergency stop!\n");
	}
	return (EINTR);
}

#else /* NSERVO == 0 */

int
sys_cncmove(struct proc *p, void *v, register_t *retval)
{
	return (ENODEV);
}

#endif /* NSERVO > 0 */


#if NSPOTWELDER > 0

/* Return spot welding trigger status */
int
sys_cncspotweldtrig(struct proc *p, void *v, register_t *retval)
{
	struct sys_cncspotweldtrig_args /* {
		syscallarg(int) welder;
	} */ *uap = v;
	int welder = SCARG(uap,welder);

	if (welder < 0 || welder >= cnc_nspotwelders) {
		return (ENODEV);
	}
	return spotwelder_trig(cnc_spotwelders[welder]);
}

/* Return spot welding select switch status */
int
sys_cncspotweldselect(struct proc *p, void *v, register_t *retval)
{
	struct sys_cncspotweldselect_args /* {
		syscallarg(int) welder;
	} */ *uap = v;
	int welder = SCARG(uap,welder);

	if (welder < 0 || welder >= cnc_nspotwelders) {
		return (ENODEV);
	}
	return spotwelder_select(cnc_spotwelders[welder]);
}

/* Spot weld */
int
sys_cncspotweld(struct proc *p, void *v, register_t *retval)
{
	struct sys_cncspotweld_args /* {
		syscallarg(int) welder;
		syscallarg(int) cycles;
	} */ *uap = v;
	int welder = SCARG(uap,welder);
	int cycles = SCARG(uap,cycles);

	if (welder < 0 || welder >= cnc_nspotwelders) {
		return (ENODEV);
	}
	if (cycles < 0 || cycles > SPOTWELDER_MAXCYCLES) {
		return (EINVAL);
	}
	return spotwelder_weld(cnc_spotwelders[welder], cycles);
}

#else /* NSPOTWELDER == 0 */

int
sys_cncspotweldtrig(struct proc *p, void *v, register_t *retval)
{
	return (ENODEV);
}
int
sys_cncspotweldselect(struct proc *p, void *v, register_t *retval)
{
	return (ENODEV);
}
int
sys_cncspotweld(struct proc *p, void *v, register_t *retval)
{
	return (ENODEV);
}

#endif /* NSPOTWELDER > 0 */

int
sys_spinctl(struct proc *p, void *v, register_t *retval)
{
	return (ENOSYS);
}

int
sys_atcctl(struct proc *p, void *v, register_t *retval)
{
	return (ENOSYS);
}

int
sys_laserctl(struct proc *p, void *v, register_t *retval)
{
	return (ENOSYS);
}

int
sys_pickplacectl(struct proc *p, void *v, register_t *retval)
{
	return (ENOSYS);
}

int
sys_coolantctl(struct proc *p, void *v, register_t *retval)
{
	return (ENOSYS);
}

int
sys_estop(struct proc *p, void *v, register_t *retval)
{
	return cnc_estop_raised() ? 1 : 0;
}

int
sys_cncpos(struct proc *p, void *v, register_t *retval)
{
	struct sys_cncpos_args /* {
		syscallarg(cnc_vec_t *) vec;
	} */ *uap = v;
	cnc_vec_t *pos = SCARG(uap,vec);
	int i;

	for (i = 0; i < CNC_NAXES; i++) {
		pos->v[i] = cnc_pos.v[i];
	}
	return (0);
}
