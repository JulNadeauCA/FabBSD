/*	$FabBSD$	*/

/*
 * Copyright (c) 2009 Hypertriton, Inc. <http://hypertriton.com/>
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
 * GPIO over PC parallel port driver. The GPIO numbers are arranged to match
 * pin number on the standard host-side DB25 port.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/gpio.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/isa/isavar.h>
#include <dev/gpio/gpiovar.h>

#include <dev/ic/lptgpiovar.h>

#define LPTGPIO_DATA		0x00
#define LPTGPIO_STATUS		0x01
#define LPTGPIO_CONTROL		0x02

struct cfdriver lptgpio_cd = {
	NULL, "lptgpio", DV_DULL
};

const struct lptgpio_lpt_pin {
	int gpio;
	int reg;
	int offset;
	int inverted;
	int caps;
} lptgpio_pins[LPTGPIO_NPINS] = {
	{ 0,	LPTGPIO_CONTROL, 0, 1,	GPIO_PIN_OUTPUT },	/* nStrobe */
	{ 1,	LPTGPIO_DATA,	 0, 0,	GPIO_PIN_INPUT|GPIO_PIN_OUTPUT },
	{ 2,	LPTGPIO_DATA,	 1, 0,	GPIO_PIN_INPUT|GPIO_PIN_OUTPUT },
	{ 3,	LPTGPIO_DATA,	 2, 0,	GPIO_PIN_INPUT|GPIO_PIN_OUTPUT },
	{ 4,	LPTGPIO_DATA,	 3, 0,	GPIO_PIN_INPUT|GPIO_PIN_OUTPUT },
	{ 5,	LPTGPIO_DATA,	 4, 0,	GPIO_PIN_INPUT|GPIO_PIN_OUTPUT },
	{ 6,	LPTGPIO_DATA,	 5, 0,	GPIO_PIN_INPUT|GPIO_PIN_OUTPUT },
	{ 7,	LPTGPIO_DATA,	 6, 0,	GPIO_PIN_INPUT|GPIO_PIN_OUTPUT },
	{ 8,	LPTGPIO_DATA,	 7, 0,	GPIO_PIN_INPUT|GPIO_PIN_OUTPUT },
	{ 9,	LPTGPIO_STATUS,	 6, 0,	GPIO_PIN_INPUT },	/* nAck */
	{ 10,	LPTGPIO_STATUS,	 7, 1,	GPIO_PIN_INPUT },	/* Busy */
	{ 11,	LPTGPIO_STATUS,	 5, 0,	GPIO_PIN_INPUT },	/* Paper-Out */
	{ 12,	LPTGPIO_STATUS,	 4, 0,	GPIO_PIN_INPUT },	/* Select */
	{ 13,	LPTGPIO_CONTROL, 1, 1,	GPIO_PIN_OUTPUT },	/* Linefeed */
	{ 14,	LPTGPIO_STATUS,	 3, 0,	GPIO_PIN_INPUT },	/* nError */
	{ 15,	LPTGPIO_CONTROL, 2, 0,	GPIO_PIN_OUTPUT },	/* nInitialize */
	{ 16,	LPTGPIO_CONTROL, 3, 1,	GPIO_PIN_OUTPUT }	/* nSelect */
};

void
lptgpio_attach_common(struct lptgpio_softc *sc)
{
	struct gpiobus_attach_args gba;
	int i;

	printf("\n");

	for (i = 0; i < LPTGPIO_NPINS; i++) {
		sc->sc_gpio_pins[i].pin_num = i;
		sc->sc_gpio_pins[i].pin_caps = lptgpio_pins[i].caps;
		sc->sc_gpio_pins[i].pin_state =
		    (lptgpio_pins[i].caps & GPIO_PIN_INPUT) ?
		    lptgpio_pin_read(sc,i) : GPIO_PIN_LOW;
		
	}

	sc->sc_gpio_gc.gp_cookie = sc;
	sc->sc_gpio_gc.gp_pin_read = lptgpio_pin_read;
	sc->sc_gpio_gc.gp_pin_write = lptgpio_pin_write;
	sc->sc_gpio_gc.gp_pin_ctl = lptgpio_pin_ctl;

	gba.gba_name = "gpio";
	gba.gba_gc = &sc->sc_gpio_gc;
	gba.gba_pins = sc->sc_gpio_pins;
	gba.gba_npins = LPTGPIO_NPINS;

	(void)config_found(&sc->sc_dev, &gba, gpiobus_print);
}

int
lptgpio_pin_read(void *arg, int pin)
{
	struct lptgpio_softc *sc = arg;
	u_int8_t mask, v;

	if (pin < 0 || pin >= LPTGPIO_NPINS)
		return (0);

	mask = bus_space_read_1(sc->sc_iot, sc->sc_ioh, lptgpio_pins[pin].reg);
	v = (mask >> lptgpio_pins[pin].offset) & 0x01;
	if (lptgpio_pins[pin].inverted) {
		v = !v;
	}
	sc->sc_gpio_pins[pin].pin_state = (v) ? GPIO_PIN_HIGH : GPIO_PIN_LOW;
	return (sc->sc_gpio_pins[pin].pin_state);
}

void
lptgpio_pin_write(void *arg, int pin, int value)
{
	struct lptgpio_softc *sc = arg;
	u_int8_t v;

	if (pin < 0 || pin >= LPTGPIO_NPINS)
		return;
	
	v = bus_space_read_1(sc->sc_iot, sc->sc_ioh, lptgpio_pins[pin].reg);
	if (value == GPIO_PIN_HIGH) {
		v |= (0x01 << lptgpio_pins[pin].offset);
		sc->sc_gpio_pins[pin].pin_state = 1;
	} else {
		v &= ~(0x01 << lptgpio_pins[pin].offset);
		sc->sc_gpio_pins[pin].pin_state = 0;
	}
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, lptgpio_pins[pin].reg, v);
}

void
lptgpio_pin_ctl(void *arg, int pin, int flags)
{
}
