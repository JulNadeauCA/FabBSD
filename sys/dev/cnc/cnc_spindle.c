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
 * Spindle controller with closed-loop control (direction output,
 * tachometer input and increment/decrement output signals).
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
#include <dev/cnc/cnc_spindlevar.h>

int	spindle_match(struct device *, void *, void *);
void	spindle_attach(struct device *, struct device *, void *);
int	spindle_detach(struct device *, int);
int	spindle_activate(struct device *, enum devact);

struct cfattach spindle_ca = {
	sizeof(struct spindle_softc),
	spindle_match,
	spindle_attach,
	spindle_detach,
	spindle_activate
};

struct cfdriver spindle_cd = {
	NULL, "spindle", DV_DULL
};

int
spindle_match(struct device *parent, void *match, void *aux)
{
	struct cfdata *cf = match;

	return (strcmp(cf->cf_driver->cd_name, "spindle") == 0);
}

void
spindle_attach(struct device *parent, struct device *self, void *aux)
{
	struct spindle_softc *sc = (struct spindle_softc *)self;
	struct gpio_attach_args *ga = aux;

	/* Generic CNC device initialization. */
	if (cnc_device_attach(sc, CNC_DEVICE_SPINDLE) == -1)
		goto fail;

	/* Check that we have enough pins */
	if (gpio_npins(ga->ga_mask) != SPINDLE_NPINS) {
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
	CNC_MAP_OUTPUT(sc, SPINDLE_PIN_DIR, "DIR");
	CNC_MAP_OUTPUT(sc, SPINDLE_PIN_INCREMENT, "INCREMENT");
	CNC_MAP_OUTPUT(sc, SPINDLE_PIN_DECREMENT, "DECREMENT");
	CNC_MAP_INPUT(sc, SPINDLE_PIN_TACHO, "TACHO");

	/* Initialize outputs */
	gpio_pin_write(sc->sc_gpio, &sc->sc_map, SPINDLE_PIN_DIR, GPIO_PIN_LOW);
	gpio_pin_write(sc->sc_gpio, &sc->sc_map, SPINDLE_PIN_INCREMENT, GPIO_PIN_LOW);
	gpio_pin_write(sc->sc_gpio, &sc->sc_map, SPINDLE_PIN_DECREMENT, GPIO_PIN_LOW);

	sc->sc_speed = 0;
	sc->sc_speed_tacho = 0;
	sc->sc_speed_max = 3000;			/* XXX configure */

	printf("\n");
	return;
fail:
	gpio_pin_unmap(sc->sc_gpio, &sc->sc_map);
}

int
spindle_detach(struct device *self, int flags)
{
	return (cnc_device_detach(self));
}

int
spindle_activate(struct device *self, enum devact act)
{
	return (cnc_device_activate(self));
}

void
spindle_set_dir(void *arg, int dir)
{
	struct spindle_softc *sc = arg;

	if (sc->sc_dir != dir) {
		sc->sc_dir = dir;
		gpio_pin_write(sc->sc_gpio, &sc->sc_map, SPINDLE_PIN_DIR,
		    dir ? GPIO_PIN_HIGH : GPIO_PIN_LOW);
	}
}

void
spindle_step(void *arg, int dir)
{
	struct spindle_softc *sc = arg;

	if (sc->sc_dir != dir) {
		sc->sc_dir = dir;
		gpio_pin_write(sc->sc_gpio, &sc->sc_map, SPINDLE_PIN_DIR,
		    dir ? GPIO_PIN_HIGH : GPIO_PIN_LOW);
	}
	sc->sc_step = !sc->sc_step;
	gpio_pin_write(sc->sc_gpio, &sc->sc_map, SPINDLE_PIN_STEP,
	    sc->sc_step ? GPIO_PIN_HIGH : GPIO_PIN_LOW);
}

/* Return a spindle device by name. */
struct spindle_softc *
spindle_find(int id)
{
	if (id < 0 || id >= cnc_nspindles) {
		return (NULL);
	}
	return (cnc_spindles[id]);
}
