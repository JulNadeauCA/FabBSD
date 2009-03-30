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
	
	sc->sc_flags = 0;

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

