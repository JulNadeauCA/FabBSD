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
 * Stepper/servo motor control with step/direction signal.
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
#include <dev/cnc/cnc_servovar.h>

int	servo_match(struct device *, void *, void *);
void	servo_attach(struct device *, struct device *, void *);
int	servo_detach(struct device *, int);
int	servo_activate(struct device *, enum devact);

struct cfattach servo_ca = {
	sizeof(struct servo_softc),
	servo_match,
	servo_attach,
	servo_detach,
	servo_activate
};

struct cfdriver servo_cd = {
	NULL, "servo", DV_DULL
};

int
servo_match(struct device *parent, void *match, void *aux)
{
	struct cfdata *cf = match;

	return (strcmp(cf->cf_driver->cd_name, "servo") == 0);
}

void
servo_attach(struct device *parent, struct device *self, void *aux)
{
	struct servo_softc *sc = (struct servo_softc *)self;
	struct gpio_attach_args *ga = aux;

	/* Generic CNC device initialization. */
	if (cnc_device_attach(sc, CNC_DEVICE_SERVO) == -1)
		return;

	/* Check that we have enough pins */
	if (gpio_npins(ga->ga_mask) != SERVO_NPINS) {
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
	
	/* Configure step and direction outputs. */
	CNC_MAP_OUTPUT(sc, SERVO_PIN_STEP, "STEP");
	CNC_MAP_OUTPUT(sc, SERVO_PIN_DIR, "DIR");

	/* Some motor controllers need an initial pulse. */
	gpio_pin_write(sc->sc_gpio, &sc->sc_map, SERVO_PIN_DIR, GPIO_PIN_LOW);
	gpio_pin_write(sc->sc_gpio, &sc->sc_map, SERVO_PIN_STEP, GPIO_PIN_LOW);
	delay(100000);
	gpio_pin_write(sc->sc_gpio, &sc->sc_map, SERVO_PIN_DIR, GPIO_PIN_HIGH);
	gpio_pin_write(sc->sc_gpio, &sc->sc_map, SERVO_PIN_STEP, GPIO_PIN_HIGH);
	delay(100000);
	gpio_pin_write(sc->sc_gpio, &sc->sc_map, SERVO_PIN_DIR, GPIO_PIN_LOW);
	gpio_pin_write(sc->sc_gpio, &sc->sc_map, SERVO_PIN_STEP, GPIO_PIN_LOW);

	sc->sc_step = 0;
	sc->sc_dir = 0;
	
	printf("\n");
	return;
fail:
	gpio_pin_unmap(sc->sc_gpio, &sc->sc_map);
}

int
servo_detach(struct device *self, int flags)
{
	return (cnc_device_detach(self));
}

int
servo_activate(struct device *self, enum devact act)
{
	return (cnc_device_activate(self));
}

void
servo_set_dir(struct servo_softc *sc, int dir)
{
	if (sc->sc_dir != dir) {
		sc->sc_dir = dir;
		gpio_pin_write(sc->sc_gpio, &sc->sc_map, SERVO_PIN_DIR,
		    dir ? GPIO_PIN_HIGH : GPIO_PIN_LOW);
	}
}

void
servo_step(struct servo_softc *sc, int dir)
{
	if (sc->sc_dir != dir) {
		sc->sc_dir = dir;
		gpio_pin_write(sc->sc_gpio, &sc->sc_map, SERVO_PIN_DIR,
		    (dir == 1) ? GPIO_PIN_HIGH : GPIO_PIN_LOW);
	}
	sc->sc_step = !sc->sc_step;
	gpio_pin_write(sc->sc_gpio, &sc->sc_map, SERVO_PIN_STEP,
	    sc->sc_step ? GPIO_PIN_HIGH : GPIO_PIN_LOW);
}
