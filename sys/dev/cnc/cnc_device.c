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
 * Generic CNC device code.
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
#include <sys/kthread.h>

#include <sys/cnc.h>

#include "cnc_devicevar.h"
#include "cncvar.h"

struct cnc_deviceq cnc_devices;
int cnc_initialized = 0;

int
cnc_device_attach(void *p, enum cnc_device_type type)
{
	struct cnc_device *cd = p;

	if (!cnc_initialized) {
		TAILQ_INIT(&cnc_devices);
		cnc_initialized = 1;
	}
	cd->cd_type = type;
	cd->cd_flags = 0;
	TAILQ_INSERT_TAIL(&cnc_devices, cd, devices);

	switch (type) {
	case CNC_DEVICE_SERVO:
		if ((cnc_nservos+1) <= CNC_NAXES) {
			printf(": axis #%d\n", cnc_nservos);
			cnc_servos[cnc_nservos++] = (struct servo_softc *)cd;
		} else {
			printf(": ignored (bump CNC_NAXES)\n");
		}
		break;
	case CNC_DEVICE_SPINDLE:
		if ((cnc_nspindles+1) <= CNC_MAX_SPINDLES) {
			printf(": spindle #%d\n", cnc_nspindles);
			cnc_spindles[cnc_nspindles++] = (struct spindle_softc *)cd;
		} else {
			printf(": ignored (bump CNC_MAX_SPINDLES)\n");
		}
		break;
	case CNC_DEVICE_ESTOP:
		if ((cnc_nestops+1) <= CNC_MAX_ESTOPS) {
			printf(": estop #%d\n", cnc_nestops);
			cnc_estops[cnc_nestops++] = (struct estop_softc *)cd;
		} else {
			printf(": ignored (bump CNC_MAX_ESTOPS)\n");
		}
		break;
	case CNC_DEVICE_ENCODER:
		if ((cnc_nencoders+1) <= CNC_MAX_ENCODERS) {
			printf(": encoder #%d\n", cnc_nencoders);
			cnc_encoders[cnc_nencoders++] = (struct encoder_softc *)cd;
		} else {
			printf(": ignored (bump CNC_MAX_ENCODERS)\n");
		}
		break;
	case CNC_DEVICE_MPG:
		if ((cnc_nmpgs+1) <= CNC_MAX_MPGS) {
			printf(": mpg #%d\n", cnc_nmpgs);
			cnc_mpgs[cnc_nmpgs++] = (struct mpg_softc *)cd;
		} else {
			printf(": ignored (bump CNC_MAX_MPGS)\n");
		}
		break;
	case CNC_DEVICE_LCD:
		if ((cnc_nlcds+1) <= CNC_MAX_LCDS) {
			printf(": lcd #%d\n", cnc_nlcds);
			cnc_lcds[cnc_nlcds++] = (struct cnclcd_softc *)cd;
		} else {
			printf(": ignored (bump CNC_MAX_LCDS)\n");
		}
		break;
	case CNC_DEVICE_SPOTWELDER:
		if ((cnc_nspotwelders+1) <= CNC_MAX_SPOTWELDERS) {
			printf(": spotwelder #%d\n", cnc_nspotwelders);
			cnc_spotwelders[cnc_nspotwelders++] = (struct spotwelder_softc *)cd;
		} else {
			printf(": ignored (bump CNC_MAX_SPOTWELDERS)\n");
		}
		break;
	default:
		printf(": unimplemented\n");
		break;
	}
	printf("%s", cd->cd_dev.dv_xname);
	return (0);
}

int
cnc_device_detach(void *p)
{
	struct cnc_device *cd = p;

	TAILQ_REMOVE(&cnc_devices, cd, devices);
	return (0);
}

int
cnc_device_activate(void *p)
{
	return (0);
}

