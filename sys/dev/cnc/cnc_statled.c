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
 * Status LED for CNC operations.
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
#include <dev/cnc/cnc_statledvar.h>

int	cncstatled_match(struct device *, void *, void *);
void	cncstatled_attach(struct device *, struct device *, void *);
int	cncstatled_detach(struct device *, int);
int	cncstatled_activate(struct device *, enum devact);
void	cncstatled_abort_all(void);

struct cfattach cncstatled_ca = {
	sizeof(struct cncstatled_softc),
	cncstatled_match,
	cncstatled_attach,
	cncstatled_detach,
	cncstatled_activate
};

struct cfdriver cncstatled_cd = {
	NULL, "cncstatled", DV_DULL
};

int
cncstatled_match(struct device *parent, void *match, void *aux)
{
	struct cfdata *cf = match;

	return (strcmp(cf->cf_driver->cd_name, "cncstatled") == 0);
}

void
cncstatled_attach(struct device *parent, struct device *self, void *aux)
{
	struct cncstatled_softc *sc = (struct cncstatled_softc *)self;
	struct gpio_attach_args *ga = aux;

	cnc_status_led = sc;

	if (gpio_npins(ga->ga_mask) != 1) {
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

	/* Map the output signal. */
	CNC_MAP_OUTPUT(sc, 0, "LED");

	printf("\n");
	return;
fail:
	gpio_pin_unmap(sc->sc_gpio, &sc->sc_map);
}

int
cncstatled_detach(struct device *self, int flags)
{
	return (0);
}

int
cncstatled_activate(struct device *self, enum devact act)
{
	return (0);
}

/* Set the state of the LED. */
void
cncstatled_set(int state)
{
	struct cncstatled_softc *sc;

	if ((sc = cnc_status_led) != NULL)
		gpio_pin_write(sc->sc_gpio, &sc->sc_map, 0,
	                       state ? GPIO_PIN_HIGH : GPIO_PIN_LOW);
}
