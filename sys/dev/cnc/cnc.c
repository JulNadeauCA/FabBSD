/*	$FabBSD$	*/
/*
 * Copyright (c) 2007-2009 Hypertriton, Inc. <http://www.hypertriton.com/>
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

#include "cncvar.h"
#include "cnc_capturevar.h"

#include "servo.h"
#include "spindle.h"
#include "estop.h"
#include "encoder.h"
#include "mpg.h"
#include "cnclcd.h"
#include "cncstatled.h"

#define STEPMAX	(INT64_MAX-1)
#define STEPLEN 20336			/* 20336 units = 1 step */
#define MAXSTEPS (STEPMAX/STEPLEN)

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
struct estop_softc      *cnc_estops[CNC_MAX_ESTOPS];
struct encoder_softc    *cnc_encoders[CNC_MAX_ENCODERS];
struct mpg_softc        *cnc_mpgs[CNC_MAX_MPGS];
struct cnclcd_softc     *cnc_lcds[CNC_MAX_LCDS];
struct cncstatled_softc *cnc_status_led = NULL;

int cnc_capture = 0;
int cnc_nservos = 0;
int cnc_nspindles = 0;
int cnc_nestops = 0;
int cnc_nencoders = 0;
int cnc_nmpgs = 0;
int cnc_nlcds = 0;

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
	cnc_timings.move_jog = 0;
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

/* Benchmark cnc_move_jog(). */
cnc_utime_t
cnc_calibrate_move_jog(void)
{
#if NMPG > 0
	struct timespec tv1, tv2;
	struct cnc_velocity vp;
	cnc_utime_t r;
	int i, s;

	if (cnc_nmpgs < 1)
		return (1);
	
	printf("cnc: simulating cnc_move_jog()...");

	vp.v0 = 100;
	vp.F = 10000;
	vp.Amax = 1234;
	vp.Jmax = 1234;
	for (i = 0; i < CNC_NAXES; i++)
		cnc_pos.v[i] = 0.0;

	/* Benchmark the routine using the system clock. */
	s = splhigh();
	nanotime(&tv1);
#if 0
	/* TODO */
	sys_cncjog(NULL, &insn, 1);
#endif
	nanotime(&tv2);
	splx(s);

	r = (cnc_utime_t)(tv2.tv_sec - tv1.tv_sec)*1e12 +
	                 (tv2.tv_nsec - tv1.tv_nsec);
	printf("...%lluns\n", r);
	return (r);
#else
	return (1);
#endif
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
			printf("cnc: SETPOS axis#%d: %lu -> %lu\n", i,
			    cnc_pos.v[i], pos->v[i]);
			cnc_pos.v[i] = pos->v[i];
		}
		return (0);
	case CNC_GETDEVICEINFO:
		{
			struct cnc_device_info *di = (struct cnc_device_info *)data;
			di->nservos = cnc_nservos;
			di->nspindles = cnc_nspindles;
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
		if (cnc_timings.move_jog == 0) {
			cnc_timings.move_jog = cnc_calibrate_move_jog();
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
	default:
		break;
	}
	return (ENODEV);
}

/* Return 1 if one of the registered e-stops are raised. */
int
cnc_estop_raised(void)
{
#if NESTOP > 0
	int i;
	for (i = 0; i < cnc_nestops; i++) {
		if (estop_get_state(cnc_estops[i])) {
			cnc_stats.estops++;
			return (1);
		}
	}
#endif
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
	const struct cnc_velocity *velprof = SCARG(uap,vel);
	const cnc_vec_t *v2 = SCARG(uap,tgt);
	cnc_real_t t;
	cnc_vec_t d, v1=cnc_pos;
	cnc_pos_t inc, incMin, vMajor=0;
	struct cnc_quintic_profile Q;
	int dir[CNC_NAXES], s, i, nonzero=0, axisMajor=0;
	
	if (cnc_calibrated())
		return (ENXIO);

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
	if (cnc_quintic_init(&Q,
	    (cnc_real_t)d.v[axisMajor],
	    (cnc_real_t)velprof->v0,
	    (cnc_real_t)velprof->F,
	    (cnc_real_t)velprof->Amax,
	    (cnc_real_t)velprof->Jmax) == -1)
		return (EINVAL);

	/*
	 * Enter the signal generation loop. We iterate over the displacement
	 * of the major axis.
	 */
	if (!cnc_capture) { s = splhigh(); }
	for (inc=0, t=0.0;
	     inc < d.v[axisMajor] && t < MAXSTEPS;
	     inc++, t+=1.0) {
		cnc_real_t v;
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
		v = cnc_quintic_step(&Q,
		    t*(Q.Ta + Q.To)/((cnc_real_t)d.v[axisMajor]))*Q.F /
		    (Q.v0 + Q.v1 + Q.v2) +
		    Q.v0;
		if (v > 0) {
			if (v > cnc_stats.peak_velocity) {
				cnc_stats.peak_velocity = v;
			}
			delay = (cnc_utime_t)(cnc_timings.hz/v);
			for (j = 0; j < delay; j++)
				;;
		}
	}
	if (!cnc_capture) { splx(s); }
	return (0);
stop:
	if (!cnc_capture) { splx(s); }
	printf("cnc: emergency stop!\n");
	return (EINTR);
}

#else /* NSERVO == 0 */

int
sys_cncmove(struct proc *p, void *v, register_t *retval)
{
	return (ENODEV);
}

#endif /* NSERVO > 0 */

#if (NSERVO > 0) && (NMPG > 0)

/*
 * Control the servos stepwise using an mpg(4) device. The kinematic limits
 * are not applied in this case.
 */
int
sys_cncjogstep(struct proc *p, void *v, register_t *retval)
{
	int i, s;

	if (cnc_nmpgs < 1)
		return (ENXIO);

	/* Fetch the initial quadrature signal state. */
	for (i = 0; i < cnc_nmpgs; i++)
		mpg_jog_init(cnc_mpgs[i]);
	
	/*
	 * Loop translating the quadrature signal to directly to step
	 * by step motion.
	 */
	s = splhigh();
	while (!cnc_estop_raised()) {
		for (i = 0; i < cnc_nmpgs; i++) {
			struct mpg_softc *mpg = cnc_mpgs[i];
			struct mpg_axis *axis;
			int axisIdx;

			axisIdx = mpg_get_axis(mpg);
			axis = &mpg->sc_axes[axisIdx];
			axis->A = gpio_pin_read(mpg->sc_gpio, &mpg->sc_map, MPG_PIN_A);
			axis->B = gpio_pin_read(mpg->sc_gpio, &mpg->sc_map, MPG_PIN_B);

			if (axis->A == axis->Aprev &&
			    axis->B == axis->Bprev)
				continue;

			if (MPG_TRANSITION_CW(axis)) { cnc_inc_axis(axisIdx, 1); }
			if (MPG_TRANSITION_CCW(axis)) { cnc_inc_axis(axisIdx, -1); }
			
			axis->Aprev = axis->A;
			axis->Bprev = axis->B;
		}
	}
	splx(s);
	return (0);
}

/*
 * Let the operator manually move to a target position using an mpg(4) device.
 * The position is moved by the specified multiplier and kinematic limits
 * are applied.
 *
 * TODO allow other types of input devices (buttons, joysticks)
 */
int
sys_cncjog(struct proc *p, void *v, register_t *retval)
{
	struct sys_cncjog_args /* {
		syscallarg(const struct cnc_velocity *) vel;
	} */ *uap = v;
	const struct cnc_velocity *velprof = SCARG(uap,vel);
	struct mpg_softc *mpg;
	struct cnc_quintic_profile Q, Qprev;
	cnc_vec_t vTgt, vTgtLast;
	cnc_real_t t = 0.0, dt = 0.0, L;
	int dir, axis, s;
	cnc_utime_t Telapsed = 0;
	int moving = 0;
	int mult = 10;			/* XXX TODO select */

	if (cnc_calibrated()) {
		return (ENXIO);
	}
	if (cnc_nmpgs < 1) {
		printf("cnc: JOG: No MPGs\n");
		return (ENXIO);
	}
	mpg = cnc_mpgs[0];		/* TODO allow multiple mpgs */
	mpg_jog_init(mpg);

	/* Initialize the target position. */
	vTgt = cnc_pos;
	vTgtLast = vTgt;
	Q.Ta = 0.0;
	Q.To = 0.0;

	/* Enter the jog loop. */
	s = splhigh();
	while (!cnc_estop_raised()) {
		/*
		 * Use the MPG to control the target position, fetch
		 * the currently selected axis.
		 */
		mpg_jog(mpg, &vTgt, mult);
		axis = mpg->sc_sel_axis;

		/* Compute direction and distance to target. */
		L = (cnc_real_t)(vTgt.v[axis] - cnc_pos.v[axis]);
		if (L < 0.0) { L = -L; }
		dir = (vTgt.v[axis] > cnc_pos.v[axis]) ? +1 : -1;

		/*
		 * If the target position has changed, recompute the time
		 * constants and time increment.
		 */
		if (!cnc_vec_same(&vTgtLast, &vTgt)) {
			cnc_real_t t7prev;

			if (!moving) {
				moving = 1;
				if (!simulate) {
					printf("%s: JOG: Moving to %s=%lld\n",
					    ((struct device *)mpg)->dv_xname,
					    cnc_axis_names[axis], vTgt.v[axis]);
					cnc_message("Moving ");
					cnc_message(cnc_axis_names[axis]);
					cnc_message("\n");
				}
			}
			vTgtLast = vTgt;
			Qprev = Q;
			if (cnc_quintic_init(&Q, L,
			    velprof->v0,
			    velprof->F,
			    velprof->Amax,
			    velprof->Jmax) == -1) {
				goto fail;
			}
			dt = (Q.Ta+Q.To)/L;

			printf("cnc: Quintic: L=%s ", cnc_fmt_real(L));
			printf("dt=%s ", cnc_fmt_real(dt));
			printf("F=%s ", cnc_fmt_real(Q.F));
			printf("v0=%s ", cnc_fmt_real(Q.v0));
			printf("v1=%s ", cnc_fmt_real(Q.v1));
			printf("v2=%s ", cnc_fmt_real(Q.v2));
			printf("Aref=%s ", cnc_fmt_real(Q.Aref));
			printf("[Ts=%s", cnc_fmt_real(Q.Ts));
			printf(" Ta=%s", cnc_fmt_real(Q.Ta));
			printf(" To=%s]\n", cnc_fmt_real(Q.To));

			t = 0.0;
			Telapsed = 0;
			if (t7prev >= 1e-6) {
				printf("cnc: Rescaling t for (Ta+To)=%s: ",
				    cnc_fmt_real(Q.Ta+Q.To));
				t = t/t7prev*(Q.Ta+Q.To); /* Rescale time */
				printf("t=%ss\n", cnc_fmt_real(t));
				Telapsed = 0;
			}
		}

		/*
		 * Move one step to the target position if the velocity profile
		 * has determined that this iteration must move one step.
		 */
		if (moving) {
			if (L > CNC_POS_ERROR) {
				cnc_utime_t v;		/* Steps/second */
				int clipped = 0;

				v = (cnc_utime_t)cnc_quintic_step(&Q, t);
				if (v > (cnc_real_t)cnc_kinlimits.Fmax) {
					v = (cnc_real_t)cnc_kinlimits.Fmax;
					clipped = 1;
				}
				if (v < Q.v0) {
					v = Q.v0;
					clipped = =1;
				}
#if NCNCSTATLED > 0
				cncstatled_set(clipped);
#endif
				/*
				 * Compare the estimated time elapsed (ns)
				 * to the current velocity (steps/sec).
				 */
				if ((Telapsed += cnc_timings.move_jog) >
				    1000000000UL/v) {
					Telapsed = 0;
					if (!simulate) {
						cnc_inc_axis(axis, dir);
					} else {
						cnc_pos.v[axis] += dir;
					}
					t += dt;
				}
			} else {
				if (!simulate) {
					printf("%s: JOG: Reached target %s=%lld ",
					    ((struct device *)mpg)->dv_xname,
					    cnc_axis_names[axis], cnc_pos.v[axis]);
					printf("in %s seconds\n", cnc_fmt_real(t));
				}
				t = 0.0;
				moving = 0;
			}
		}

		if (simulate)
			break;
	}
	splx(s);
	return (0);
fail:
	splx(s);
	return (EINVAL);
}

#else /* NMPG=0 or NSERVO=0 */

int
sys_cncjog(struct proc *p, void *v, register_t *retval)
{
	return (ENODEV);
}

int
sys_cncjogstep(struct proc *p, void *v, register_t *retval)
{
	return (ENODEV);
}

#endif /* NMPG>0 and NSERVO>0 */

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
