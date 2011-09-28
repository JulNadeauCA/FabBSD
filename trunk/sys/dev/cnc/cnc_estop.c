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
 * Driver for emergency stop button.
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

#include <sys/gpio.h>
#include <sys/cnc.h>

#include <dev/gpio/gpiovar.h>

#include <dev/cnc/cnc_devicevar.h>
#include <dev/cnc/cncvar.h>
#include <dev/cnc/cnc_estopvar.h>

#define ESTOPUNIT(x) minor(x)

int	estop_match(struct device *, void *, void *);
void	estop_attach(struct device *, struct device *, void *);
int	estop_detach(struct device *, int);
int	estop_activate(struct device *, enum devact);
void	estop_abort_all(void);

struct cfattach estop_ca = {
	sizeof(struct estop_softc),
	estop_match,
	estop_attach,
	estop_detach,
	estop_activate
};

struct cfdriver estop_cd = {
	NULL, "estop", DV_DULL
};

int
estop_match(struct device *parent, void *match, void *aux)
{
	struct cfdata *cf = match;

	return (strcmp(cf->cf_driver->cd_name, "estop") == 0);
}

void
estop_attach(struct device *parent, struct device *self, void *aux)
{
	struct estop_softc *sc = (struct estop_softc *)self;
	struct gpio_attach_args *ga = aux;

	/* Generic CNC device initialization. */
	if (cnc_device_attach(sc, CNC_DEVICE_ESTOP) == -1)
		return;
	
	sc->sc_flags = sc->sc_cdev.cd_dev.dv_cfdata->cf_flags;

	if (gpio_npins(ga->ga_mask) != ESTOP_NPINS) {
		printf(": invalid pin mask\n");
		return;
	}
	sc->sc_gpio = ga->ga_gpio;
	sc->sc_map.pm_map = sc->__map;
	if (gpio_pin_map(sc->sc_gpio, ga->ga_offset, ga->ga_mask,
	    &sc->sc_map)) {
		printf(": can't map pins\n");
		return;
	}

	/* Map the input signal. */
	CNC_MAP_INPUT(sc, ESTOP_PIN, "ESTOP");

	sc->sc_open = 0;
	sc->sc_queued = 0;
	
	if (sc->sc_flags & ESTOP_DEBOUNCE) {
		printf(" debounce");
	}
	printf("\n");
	return;
fail:
	gpio_pin_unmap(sc->sc_gpio, &sc->sc_map);
}

int
estop_detach(struct device *self, int flags)
{
	return (0);
}

int
estop_activate(struct device *self, enum devact act)
{
	return (0);
}

/* Return e-stop status. */
int
estop_get_state(struct estop_softc *sc)
{
	return (gpio_pin_read(sc->sc_gpio, &sc->sc_map, ESTOP_PIN));
}

/* Return e-stop status with software debounce. */
int
estop_debounce(struct estop_softc *sc)
{
	int j;
	cnc_utime_t delay, k;

	delay = (cnc_utime_t)cnc_timings.hz/100;    /* 10ms */
	for (j = 0; j < 10; j++) {
		if (!estop_get_state(sc)) {
			break;
		}
		for (k = 0; k < delay; k++)
			;;
	}
	if (j == 10) {
		return (1);
	}
	return (0);
}

int
estopopen(dev_t dev, int flag, int mode, struct proc *p)
{
	int unit = ESTOPUNIT(dev);
	struct estop_softc *sc;

	if (unit >= estop_cd.cd_ndevs)
		return (ENXIO);

	sc = estop_cd.cd_devs[unit];
	if (sc == NULL) {
		return (ENXIO);
	}
	if (sc->sc_open) {
		return (EBUSY);
	}
	sc->sc_open = 1;
	return (0);
}

int
estopclose(dev_t dev, int flag, int mode, struct proc *p)
{
	int unit = ESTOPUNIT(dev);
	struct estop_softc *sc = estop_cd.cd_devs[unit];

	sc->sc_open = 0;
	sc->sc_queued = 0;
	return (0);
}

int
estopread(dev_t dev, struct uio *uio, int flag)
{
	int unit = ESTOPUNIT(dev);
	struct estop_softc *sc = estop_cd.cd_devs[unit];
	struct cnc_input_event ev;
	int s;
	
	ev.type = CNC_EVENT_BUTTONDOWN;
	ev.which = 0;

	if (sc->sc_queued) {
		sc->sc_queued = 0;
		ev.data = 1;
	} else {
		s = splhigh();
		ev.data = estop_get_state(sc);
		splx(s);
	}
	return uiomove((caddr_t)&ev, sizeof(struct cnc_input_event), uio);
}

int
estopioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	return (ENXIO);
}
