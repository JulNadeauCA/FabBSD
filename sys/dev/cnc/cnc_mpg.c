/*
 * Copyright (c) 2009 Hypertriton, Inc. <http://www.hypertriton.com/>
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
		sc->sc_axes[i].A = 0;
		sc->sc_axes[i].B = 0;
		sc->sc_axes[i].Aprev = 0;
		sc->sc_axes[i].Bprev = 0;
		sc->sc_pulses[i] = 0;
	}

	sc->sc_sel_axis = 0;
	sc->sc_mult = 1000;
	sc->sc_ppi = 4;

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

void
mpg_jog_init(struct mpg_softc *sc)
{
	int j;

	for (j = 0; j < sc->sc_naxes; j++) {
		struct mpg_axis *axis = &sc->sc_axes[j];

		axis->Aprev = gpio_pin_read(sc->sc_gpio, &sc->sc_map, MPG_PIN_A);
		axis->Bprev = gpio_pin_read(sc->sc_gpio, &sc->sc_map, MPG_PIN_B);
	}
}

/*
 * Scan the registered MPGs for changes in their quadrature signal states,
 * and decode the transitions into position changes. Move vTgt by an
 * amount dictated by the current pulse multiplier setting. The currently
 * selected axis is returned into a.
 */
void
mpg_jog(struct mpg_softc *sc, cnc_vec_t *vTgt, int mult)
{
	int axisIdx;
	struct mpg_axis *axis;

	if (sc->sc_sel_axis != (axisIdx = mpg_get_axis(sc))) {
		printf("%s: Selected axis%d -> axis%d\n",
		    ((struct device *)sc)->dv_xname, sc->sc_sel_axis, axisIdx);
		sc->sc_sel_axis = axisIdx;
		*vTgt = cnc_pos;			/* Reset position */
	}
	axis = &sc->sc_axes[axisIdx];
	axis->A = gpio_pin_read(sc->sc_gpio, &sc->sc_map, MPG_PIN_A);
	axis->B = gpio_pin_read(sc->sc_gpio, &sc->sc_map, MPG_PIN_B);

	if (axis->A == axis->Aprev &&
	    axis->B == axis->Bprev)
		return;

	if (( axis->A && !axis->B && !axis->Aprev && !axis->Bprev) ||
	    ( axis->A &&  axis->B &&  axis->Aprev && !axis->Bprev) ||
	    (!axis->A &&  axis->B &&  axis->Aprev &&  axis->Bprev) ||
	    (!axis->A && !axis->B && !axis->Aprev &&  axis->Bprev)) {
		sc->sc_pulses[axisIdx]++;
	} else {
		sc->sc_pulses[axisIdx]--;
	}
	if (sc->sc_pulses[axisIdx] >= sc->sc_ppi) {
		sc->sc_pulses[axisIdx] = 0;
		vTgt->v[axisIdx] += (mult != -1) ? mult : sc->sc_mult;
	}
	if (sc->sc_pulses[axisIdx] <= -sc->sc_ppi) {
		sc->sc_pulses[axisIdx] = 0;
		vTgt->v[axisIdx] -= (mult != -1) ? mult : sc->sc_mult;
	}

	axis->Aprev = axis->A;
	axis->Bprev = axis->B;
}