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
 * Device generating a quadrature signal. This includes linear encoders,
 * micrometers, digital potentiometers and manual pulse generators.
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
#include <dev/cnc/cnc_encodervar.h>

int	encoder_match(struct device *, void *, void *);
void	encoder_attach(struct device *, struct device *, void *);
int	encoder_detach(struct device *, int);
int	encoder_activate(struct device *, enum devact);

struct cfattach encoder_ca = {
	sizeof(struct encoder_softc),
	encoder_match,
	encoder_attach,
	encoder_detach,
	encoder_activate
};

struct cfdriver encoder_cd = {
	NULL, "encoder", DV_DULL
};

int
encoder_match(struct device *parent, void *match, void *aux)
{
	struct cfdata *cf = match;

	return (strcmp(cf->cf_driver->cd_name, "encoder") == 0);
}

void
encoder_attach(struct device *parent, struct device *self, void *aux)
{
	struct encoder_softc *sc = (struct encoder_softc *)self;
	struct gpio_attach_args *ga = aux;

	/* Generic CNC device initialization. */
	if (cnc_device_attach(sc, CNC_DEVICE_ENCODER) == -1)
		return;

	/* Check that we have enough pins */
	if (gpio_npins(ga->ga_mask) != ENCODER_NPINS) {
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
	
	/* Configure quadrature signal input pins. */
	CNC_MAP_INPUT(sc, ENCODER_PIN_A, "A");
	CNC_MAP_INPUT(sc, ENCODER_PIN_B, "B");
	
	printf("\n");
	return;
fail:
	gpio_pin_unmap(sc->sc_gpio, &sc->sc_map);
}

int
encoder_detach(struct device *self, int flags)
{
	return (cnc_device_detach(self));
}

int
encoder_activate(struct device *self, enum devact act)
{
	return (cnc_device_activate(self));
}

/* Update encoder status; called from cnc_update(). */
void
encoder_update(struct encoder_softc *sc)
{
	int a, b;

	a = gpio_pin_read(sc->sc_gpio, &sc->sc_map, ENCODER_PIN_A);
	b = gpio_pin_read(sc->sc_gpio, &sc->sc_map, ENCODER_PIN_B);

	if (sc->sc_state_a != a) {
		printf("A changed to %d\n", a);
		sc->sc_state_a = a;
	}
	if (sc->sc_state_b != b) {
		printf("B changed to %d\n", b);
		sc->sc_state_b = b;
	}
}
