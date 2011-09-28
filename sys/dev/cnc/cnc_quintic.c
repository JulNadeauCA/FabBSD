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
 * Quintic velocity profile generation.
 */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <sys/gpio.h>
#include <sys/cnc.h>

#include <dev/gpio/gpiovar.h>

#include "cnc_devicevar.h"
#include "cncvar.h"

/* #define CNC_QUINTIC_DEBUG */

/*
 * Initialize constants to values that will yield an optimal time-energy
 * velocity profile under acceleration and jerk constraints.
 */
int
cnc_quintic_init(struct cnc_quintic_profile *q, cnc_real_t L, cnc_real_t v0,
    cnc_real_t F, cnc_real_t Amax, cnc_real_t Jmax)
{
	cnc_real_t AmaxP2 = Amax*Amax;
	cnc_real_t AmaxP3 = AmaxP2*Amax;
	cnc_real_t AmaxP4 = AmaxP3*Amax;
	cnc_real_t JmaxP2 = Jmax*Jmax;
	cnc_real_t F_P2 = F*F;
	cnc_real_t F_P3 = F_P2*F;
	cnc_real_t x, y;

	q->F = F;

	if (F >= CNC_PI_2*(AmaxP2/Jmax) ) {
		/*
		 * Feedrate F is achievable by using maximum acceleration
		 * and maximum jerk.
		 */
		x = F_P2/Amax + CNC_PI_2*((Amax*F)/Jmax);
		y = (CNC_PI_P2/2.0)*(AmaxP3/JmaxP2);
		if (L >= x) {
			/*
			 * Feedrate F is achievable at Amax acceleration.
			 */
#ifdef CNC_QUINTIC_DEBUG
			printf("cnc: F achievable at Amax (#1.1)\n");
#endif
			q->Ts = CNC_PI_2*(Amax/Jmax);
			q->Ta = q->Ts + (F/Amax);
			q->To = L/F;
		} else if (L < x && L >= y) {
			/*
			 * Feedrate F is unachievable, Amax is achievable.
			 */
#ifdef CNC_QUINTIC_DEBUG
			printf("cnc: F is unachievable, but Amax is (#1.2)\n");
#endif
			q->Ts = CNC_PI_2*(Amax/Jmax);
			q->Ta = ( (-CNC_PI*AmaxP2 + sqrt(CNC_PI_P2*AmaxP4 + 16.0*Amax*JmaxP2*L) ) /
			        (4.0*Jmax*Amax) ) + q->Ts;
			q->To = q->Ta;
		} else if (L <= y) {
			/*
			 * Neither F nor Amax are achievable.
			 */
#ifdef CNC_QUINTIC_DEBUG
			printf("cnc: neither F nor Amax are achievable (#1.3)\n");
#endif
			q->Ts = cbrt( (CNC_PI*L)/(4.0*Jmax) );
			q->Ta = 2.0*q->Ts;
			q->To = q->Ta;
		}
	} else if (F < CNC_PI_2*(AmaxP2/Jmax) ) {
		x = sqrt( (2.0*CNC_PI*F_P3)/Jmax );
		/*
		 * Feedrate F is achievable without using maximum
		 * acceleration.
		 */
		if (L >= x) {
			/*
			 * F is achievable, Amax is unachievable.
			 */
#ifdef CNC_QUINTIC_DEBUG
			printf("cnc: F is achievable, but Amax is not (#2.1)\n");
#endif
			q->Ts = sqrt( (CNC_PI*F)/(2.0*Jmax) );
			q->Ta = 2.0*q->Ts;
			q->To = L/F;
		} else if (L < x) {
			/*
			 * Neither F nor Amax are achievable.
			 */
#ifdef CNC_QUINTIC_DEBUG
			printf("cnc: neither F nor Amax are achievable (#2.2)\n");
#endif
			q->Ts = cbrt( (CNC_PI*L)/(4.0*Jmax) );
			q->Ta = 2.0*q->Ts;
			q->To = q->Ta;
		} else {
			printf("cnc: Illegal parameters to CNC_MOVE\n");
			return (-1);
		}
	}

	x = CNC_PI/(2.0*q->Ts);
	q->Aref = L/((q->Ta - q->Ts)*q->To);
	q->v0 = v0;
	q->v1 = (q->Aref/2.0)*(q->Ts - sin(2.0*x*q->Ts)/(2.0*x) );
	q->v2 = q->Aref*(q->Ta - 2.0*q->Ts) + q->v1;
	return (0);
}

/*
 * Evaluate the velocity at t under the specified quintic velocity profile.
 * Returns computed velocity in steps/second.
 */
cnc_real_t
cnc_quintic_step(const struct cnc_quintic_profile *q, cnc_real_t t)
{
	cnc_real_t v = q->v0;
	cnc_real_t t2, t5, t6;
	cnc_real_t k = CNC_PI/(2.0*q->Ts);
	
	t2 = q->Ta - q->Ts;
	t5 = q->To + q->Ts;
	t6 = q->To + q->Ta - q->Ts;

	if (t <= q->Ts) {
		v += (q->Aref/2.0)*( t - sin(2.0*k*t)/(2.0*k) );
	} else if (t <= (q->Ta - q->Ts)) {
		v += q->Aref*(t - q->Ts) + q->v1;
	} else if (t <= q->Ta) {
		v += (q->Aref/2.0)*( (t-t2) + sin(2.0*k*(t-t2))/(2.0*k) ) +
		     q->v2;
	} else if (t <= q->To) {
		v += (q->v1 + q->v2);
	} else if (t <= t5) {
		v += (q->v1 + q->v2);
		v += -(q->Aref/2.0)*( (t - q->To) -
		                       sin(2.0*k*(t - q->To))/(2.0*k) );
	} else if (t <= t6) {
		v += -q->Aref*(t-t5) + q->v2;
	} else {
		v += -(q->Aref/2.0)*( (t-t6) + sin(2.0*k*(t-t6))/(2.0*k) ) +
		     q->v1;
	}
	if (v < 10.0) {
		v = 10.0;
	}
	return (v);
}
