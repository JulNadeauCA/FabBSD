/*
 * Copyright (c) 2007-2010 Hypertriton, Inc. <http://www.hypertriton.com/>
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
 * Automatic tool changer interface.
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
#include <dev/cnc/cnc_atcvar.h>

int	atc_match(struct device *, void *, void *);
void	atc_attach(struct device *, struct device *, void *);
int	atc_detach(struct device *, int);
int	atc_activate(struct device *, enum devact);

struct cfattach atc_ca = {
	sizeof(struct atc_softc),
	atc_match,
	atc_attach,
	atc_detach,
	atc_activate
};

struct cfdriver atc_cd = {
	NULL, "atc", DV_DULL
};

int
atc_match(struct device *parent, void *match, void *aux)
{
	struct cfdata *cf = match;

	return (strcmp(cf->cf_driver->cd_name, "atc") == 0);
}

void
atc_attach(struct device *parent, struct device *self, void *aux)
{
	struct atc_softc *sc = (struct atc_softc *)self;
	struct gpio_attach_args *ga = aux;

	/* Generic CNC device initialization. */
	if (cnc_device_attach(sc, CNC_DEVICE_ATC) == -1)
		goto fail;

	/* Check that we have enough pins */
	if (gpio_npins(ga->ga_mask) != ATC_NPINS) {
		printf(": invalid pin mask\n");
		return;
	}

	/* Map pins */
	sc->sc_gpio = ga->ga_gpio;
	sc->sc_map.pm_map = sc->__map;
	if (gpio_pin_map(sc->sc_gpio, ga->ga_offset, ga->ga_mask,
	    &sc->sc_map)) {
		printf(": can't map pins\n");
		return;
	}

	/* Map the pins */
	CNC_MAP_OUTPUT(sc, ATC_PIN_PREP_NEXT, "PREP_NEXT");
	CNC_MAP_OUTPUT(sc, ATC_PIN_PREP_PREV, "PREP_PREV");
	CNC_MAP_OUTPUT(sc, ATC_PIN_CHANGE, "CHANGE");
	CNC_MAP_INPUT(sc, ATC_PIN_READY, "READY");

	/* Initialize outputs */
	gpio_pin_write(sc->sc_gpio, &sc->sc_map, ATC_PIN_PREP_NEXT, GPIO_PIN_LOW);
	gpio_pin_write(sc->sc_gpio, &sc->sc_map, ATC_PIN_PREP_PREV, GPIO_PIN_LOW);
	gpio_pin_write(sc->sc_gpio, &sc->sc_map, ATC_PIN_CHANGE, GPIO_PIN_LOW);

	sc->sc_nslots = 0;
	sc->sc_curslot = 0;

	printf("\n");
	return;
fail:
	gpio_pin_unmap(sc->sc_gpio, &sc->sc_map);
}

int
atc_detach(struct device *self, int flags)
{
	return (cnc_device_detach(self));
}

int
atc_activate(struct device *self, enum devact act)
{
	return (cnc_device_activate(self));
}

static void
WaitReady(struct atc_softc *sc)
{
	int rdy;

	for (;;) {
		rdy = gpio_pin_read(sc->sc_gpio, &sc->sc_map, ATC_PIN_READY);
		if (rdy)
			break;
	}
	/* TODO: Timeout */
}

/* Immediately change to the specified tool. */
int
atc_toolchange(void *arg, int s)
{
	struct atc_softc *sc = arg;
	int i, rdy;

	if (s < 0 || s >= sc->sc_nslots) {
		return (-1);
	}
	if (sc->sc_curslot == s) {
		return (0);
	}
	if (sc->sc_curslot < s) {
		while (abs(sc->sc_curslot - s) > 0) {
			if (s < sc->sc_curslot) {
				printf("%s: Selecting previous tool\n",
				    ((struct device *)sc)->dv_xname);
				gpio_pin_write(sc->sc_gpio, &sc->sc_map,
				    ATC_PIN_PREP_PREV, GPIO_PIN_HIGH);
			} else {
				printf("%s: Selecting next tool\n",
				    ((struct device *)sc)->dv_xname);
				gpio_pin_write(sc->sc_gpio, &sc->sc_map,
				    ATC_PIN_PREP_NEXT, GPIO_PIN_HIGH);
			}
			WaitReady();
		}
	}
}

/* Return a atc device by name. */
struct atc_softc *
atc_find(int id)
{
	if (id < 0 || id >= cnc_natcs) {
		return (NULL);
	}
	return (cnc_atcs[id]);
}
