/*
 * Copyright (c) 2009-2010 Hypertriton, Inc. <http://www.hypertriton.com/>
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
 * Manual pulse generator with quadrature signal output and optional
 * axis selection.
 */

#include <sys/param.h>
#include <sys/queue.h>
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
#include <dev/cnc/cnc_mpgvar.h>

#define MPGUNIT(x) minor(x)

int	mpg_match(struct device *, void *, void *);
void	mpg_attach(struct device *, struct device *, void *);
int	mpg_detach(struct device *, int);
int	mpg_activate(struct device *, enum devact);

struct cfattach mpg_ca = {
	sizeof(struct mpg_softc),
	mpg_match,
	mpg_attach,
	mpg_detach,
	mpg_activate
};

struct cfdriver mpg_cd = {
	NULL, "mpg", DV_DULL
};

int
mpg_match(struct device *parent, void *match, void *aux)
{
	struct cfdata *cf = match;

	return (strcmp(cf->cf_driver->cd_name, "mpg") == 0);
}

void
mpg_attach(struct device *parent, struct device *self, void *aux)
{
	struct mpg_softc *sc = (struct mpg_softc *)self;
	struct gpio_attach_args *ga = aux;
	int npins, i;

	/* Generic CNC device initialization. */
	if (cnc_device_attach(sc, CNC_DEVICE_MPG) == -1)
		return;

	/* Check that we have enough pins */
	npins = gpio_npins(ga->ga_mask);
	if (npins < 2 || npins > MPG_PIN_LAST) {
		printf(": invalid pin count\n");
		return;
	}

	/* Map pins */
	sc->sc_gpio = ga->ga_gpio;
	sc->sc_map.pm_map = sc->__map;
	if (gpio_pin_map(sc->sc_gpio, ga->ga_offset, ga->ga_mask, &sc->sc_map)) {
		printf(": can't map pins\n");
		return;
	}
	
	/* Configure quadrature signal input pins. */
	CNC_MAP_INPUT(sc, MPG_PIN_A, "A");
	CNC_MAP_INPUT(sc, MPG_PIN_B, "B");

	/* Use the remaining pins for axis selection. */
	sc->sc_naxes = npins - 2;

	/* Optional axis selection pins. */
	if (sc->sc_naxes >= 1) { CNC_MAP_INPUT(sc, MPG_PIN_SELX, "SELX"); }
	if (sc->sc_naxes >= 2) { CNC_MAP_INPUT(sc, MPG_PIN_SELY, "SELY"); }
	if (sc->sc_naxes >= 3) { CNC_MAP_INPUT(sc, MPG_PIN_SELZ, "SELZ"); }
	if (sc->sc_naxes >= 4) { CNC_MAP_INPUT(sc, MPG_PIN_SELA, "SELA"); }
	if (sc->sc_naxes >= 5) { CNC_MAP_INPUT(sc, MPG_PIN_SELB, "SELB"); }
	if (sc->sc_naxes >= 6) { CNC_MAP_INPUT(sc, MPG_PIN_SELC, "SELC"); }

	for (i = 0; i < sc->sc_naxes; i++) {
		struct mpg_axis *axis = &sc->sc_axes[i];

		axis->A = 0;
		axis->B = 0;
		axis->Aprev = gpio_pin_read(sc->sc_gpio, &sc->sc_map, MPG_PIN_A);
		axis->Bprev = gpio_pin_read(sc->sc_gpio, &sc->sc_map, MPG_PIN_B);
		sc->sc_pulses[i] = 0;
	}

	sc->sc_sel_axis = 0;
	sc->sc_mult = 1000;
	sc->sc_open = 0;

	printf("\n");
	return;
fail:
	gpio_pin_unmap(sc->sc_gpio, &sc->sc_map);
}

int
mpg_detach(struct device *self, int flags)
{
	return (cnc_device_detach(self));
}

int
mpg_activate(struct device *self, enum devact act)
{
	return (cnc_device_activate(self));
}

/* Return the currently selected axis */
int
mpg_get_axis(struct mpg_softc *sc)
{
	int x, y, z;

	switch (sc->sc_naxes) {
	case 1:
		x = gpio_pin_read(sc->sc_gpio, &sc->sc_map, MPG_PIN_SELX);
		if (x) { return (CNC_X); }
		break;
	case 2:
		x = gpio_pin_read(sc->sc_gpio, &sc->sc_map, MPG_PIN_SELX);
		y = gpio_pin_read(sc->sc_gpio, &sc->sc_map, MPG_PIN_SELY);
		if (x) { return (CNC_X); }
		else if (y) { return (CNC_Y); }
		break;
	case 3:
		x = gpio_pin_read(sc->sc_gpio, &sc->sc_map, MPG_PIN_SELX);
		y = gpio_pin_read(sc->sc_gpio, &sc->sc_map, MPG_PIN_SELY);
		z = gpio_pin_read(sc->sc_gpio, &sc->sc_map, MPG_PIN_SELZ);
		if (x) { return (CNC_X); }
		else if (y) { return (CNC_Y); }
		else if (z) { return (CNC_Z); }
		break;
	default:
		break;
	}
	return (CNC_X);
}

int
mpgopen(dev_t dev, int flag, int mode, struct proc *p)
{
	int unit = MPGUNIT(dev);
	struct mpg_softc *sc;

	if (unit >= mpg_cd.cd_ndevs)
		return (ENXIO);

	sc = mpg_cd.cd_devs[unit];
	if (sc == NULL) {
		return (ENXIO);
	}
	if (sc->sc_open) {
		return (EBUSY);
	}
	return (0);
}

int
mpgclose(dev_t dev, int flag, int mode, struct proc *p)
{
	int unit = MPGUNIT(dev);
	struct mpg_softc *sc = mpg_cd.cd_devs[unit];

	sc->sc_open = 0;
	return (0);
}

int
mpgread(dev_t dev, struct uio *uio, int flag)
{
	int unit = MPGUNIT(dev);
	struct mpg_softc *sc = mpg_cd.cd_devs[unit];
	struct cnc_input_event ev;
	struct mpg_axis *axis;
	int s;

	s = splhigh();

	ev.which = mpg_get_axis(sc);
	ev.data = 0;
	axis = &sc->sc_axes[ev.which];

	axis->A = gpio_pin_read(sc->sc_gpio, &sc->sc_map, MPG_PIN_A);
	axis->B = gpio_pin_read(sc->sc_gpio, &sc->sc_map, MPG_PIN_B);
	if (axis->A == axis->Aprev &&
	    axis->B == axis->Bprev) {
		goto out;
	}
	if (( axis->A && !axis->B && !axis->Aprev && !axis->Bprev) ||
	    ( axis->A &&  axis->B &&  axis->Aprev && !axis->Bprev) ||
	    (!axis->A &&  axis->B &&  axis->Aprev &&  axis->Bprev) ||
	    (!axis->A && !axis->B && !axis->Aprev &&  axis->Bprev)) {
		ev.data++;
		ev.type = CNC_EVENT_WHEELUP;
	} else {
		ev.data--;
		ev.type = CNC_EVENT_WHEELDOWN;
	}
	axis->Aprev = axis->A;
	axis->Bprev = axis->B;
out:
	splx(s);
	return uiomove((caddr_t)&ev, sizeof(struct cnc_input_event), uio);
}

int
mpgioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	return (ENXIO);
}
