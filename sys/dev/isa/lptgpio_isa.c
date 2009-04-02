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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/gpio.h>

#include <machine/bus.h>
#include <machine/cpufunc.h>

#include <dev/gpio/gpiovar.h>
#include <dev/isa/isavar.h>

#include <dev/ic/lptgpiovar.h>

int	lptgpio_isa_probe(struct device *, void *, void *);
void	lptgpio_isa_attach(struct device *, struct device *, void *);

struct cfattach lptgpio_isa_ca = {
	sizeof(struct lptgpio_softc), lptgpio_isa_probe, lptgpio_isa_attach
};

int
lptgpio_isa_probe(struct device *parent, void *match, void *aux)
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	bus_addr_t base;

	ia->ia_iosize = 4;
	iot = ia->ia_iot;
	base = ia->ia_iobase;
	if (bus_space_map(iot, base, ia->ia_iosize, 0, &ioh)) {
		return (0);
	}
	ia->ia_msize = 0;
	bus_space_unmap(iot, ioh, ia->ia_iosize);
	return (1);
}

void
lptgpio_isa_attach(struct device *parent, struct device *self, void *aux)
{
	struct lptgpio_softc *sc = (void *)self;
	struct isa_attach_args *ia = aux;

	sc->sc_iot = ia->ia_iot;
	if (bus_space_map(sc->sc_iot, ia->ia_iobase, 4, 0, &sc->sc_ioh))
		panic("lptgpio_isa_attach: couldn't map I/O ports");

	lptgpio_attach_common(sc);
}
