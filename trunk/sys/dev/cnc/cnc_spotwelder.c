/*
 * Copyright (c) 2011 Hypertriton, Inc. <http://www.hypertriton.com/>
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
 * Precision spot-welding controller.
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
#include <dev/cnc/cnc_spotweldervar.h>

int	spotwelder_match(struct device *, void *, void *);
void	spotwelder_attach(struct device *, struct device *, void *);
int	spotwelder_detach(struct device *, int);
int	spotwelder_activate(struct device *, enum devact);

struct cfattach spotwelder_ca = {
	sizeof(struct spotwelder_softc),
	spotwelder_match,
	spotwelder_attach,
	spotwelder_detach,
	spotwelder_activate
};

struct cfdriver spotwelder_cd = {
	NULL, "spotwelder", DV_DULL
};

int
spotwelder_match(struct device *parent, void *match, void *aux)
{
	struct cfdata *cf = match;

	return (strcmp(cf->cf_driver->cd_name, "spotwelder") == 0);
}

void
spotwelder_attach(struct device *parent, struct device *self, void *aux)
{
	struct spotwelder_softc *sc = (struct spotwelder_softc *)self;
	struct gpio_attach_args *ga = aux;

	/* Generic CNC device initialization. */
	if (cnc_device_attach(sc, CNC_DEVICE_SPOTWELDER) == -1)
		return;

	/* Check that we have enough pins */
	if (gpio_npins(ga->ga_mask) != SPOTWELDER_NPINS) {
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
	CNC_MAP_OUTPUT(sc, SPOTWELDER_PIN_RELAY, "RELAY");
	CNC_MAP_INPUT(sc, SPOTWELDER_PIN_PHASE, "PHASE");
	CNC_MAP_INPUT(sc, SPOTWELDER_PIN_TRIGGER, "TRIGGER");
	CNC_MAP_INPUT(sc, SPOTWELDER_PIN_SELECT, "SELECT");

	/* Initialize to off state. */
	gpio_pin_write(sc->sc_gpio, &sc->sc_map, SPOTWELDER_PIN_RELAY,
	    GPIO_PIN_LOW);

	printf("\n");
	return;
fail:
	gpio_pin_unmap(sc->sc_gpio, &sc->sc_map);
}

int
spotwelder_detach(struct device *self, int flags)
{
	return (cnc_device_detach(self));
}

int
spotwelder_activate(struct device *self, enum devact act)
{
	return (cnc_device_activate(self));
}

int
spotwelder_trig(struct spotwelder_softc *sc)
{
	const int debounce = 1000;
	int s, i, x;

	s = splhigh();
	for (i = 0; i < debounce; i++) {
		x = gpio_pin_read(sc->sc_gpio, &sc->sc_map,
		    SPOTWELDER_PIN_TRIGGER);
		if (x == 0)
			break;
	}
	splx(s);
	return (i == debounce);
}

int
spotwelder_select(struct spotwelder_softc *sc)
{
	const int debounce = 1000;
	int s, i, x;

	s = splhigh();
	for (i = 0; i < debounce; i++) {
		x = gpio_pin_read(sc->sc_gpio, &sc->sc_map, SPOTWELDER_PIN_SELECT);
		if (x == 0)
			break;
	}
	splx(s);
	return (i == debounce);
}

int
spotwelder_weld(struct spotwelder_softc *sc, int ncycles)
{
	int s, i, ph;

#if 0
	s = splhigh();
	gpio_pin_write(sc->sc_gpio, &sc->sc_map,
	    SPOTWELDER_PIN_RELAY, GPIO_PIN_HIGH);
	delay(100);
	gpio_pin_write(sc->sc_gpio, &sc->sc_map,
	    SPOTWELDER_PIN_RELAY, GPIO_PIN_LOW);
	splx(s);
	return (0);
#endif

	s = splhigh();
	for (;;) {
		/* Wait for AC cycle start. */
		while ((gpio_pin_read(sc->sc_gpio, &sc->sc_map,
		    SPOTWELDER_PIN_PHASE)) == 0)
			;;
		while ((gpio_pin_read(sc->sc_gpio, &sc->sc_map,
		    SPOTWELDER_PIN_PHASE)) == 1)
			;;

		/* Welder ON */
		gpio_pin_write(sc->sc_gpio, &sc->sc_map, SPOTWELDER_PIN_RELAY,
		    GPIO_PIN_HIGH);

#if 1
		/* Wait for specified number of AC cycles. */
		for (i = 0, ph = 0;
		     i < ncycles;
		     i++) {
			while ((gpio_pin_read(sc->sc_gpio, &sc->sc_map,
			    SPOTWELDER_PIN_PHASE)) == !ph)
				;;
			ph = !ph;
		}
#else
		delay(ncycles);
#endif

		/* Welder OFF */
		gpio_pin_write(sc->sc_gpio, &sc->sc_map, SPOTWELDER_PIN_RELAY,
		    GPIO_PIN_LOW);
		break;
	}
	splx(s);
	return (0);
}
