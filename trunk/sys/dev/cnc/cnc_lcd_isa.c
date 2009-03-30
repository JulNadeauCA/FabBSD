/*
 * Copyright (c) 1997 - 1999, Jason Downs.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name(s) of the author(s) nor the name OpenBSD
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*-
 * Copyright (c) 1993, 1994, 1995, 1996
 *	Charles M. Hannum.  All rights reserved.
 * Copyright (c) 1991 The Regents of the University of California.
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
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)com.c	7.5 (Berkeley) 5/16/91
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/cnc.h>

#include <machine/bus.h>

#include <dev/cnc/cncvar.h>
#include <dev/cnc/cnc_devicevar.h>
#include <dev/cnc/cnc_lcdvar.h>

#include <dev/isa/isavar.h>

#include <dev/ic/comreg.h>
#include <dev/ic/ns16550reg.h>
#define	com_lcr	com_cfcr

int cnclcd_isa_match(struct device *, void *, void *);
void cnclcd_isa_attach(struct device *, struct device *, void *);
int cnclcd_isa_detach(struct device *, int);
int cnclcd_isa_activate(struct device *, enum devact);

struct cfattach cnclcd_isa_ca = {
        sizeof(struct cnclcd_softc),
	cnclcd_isa_match,
	cnclcd_isa_attach,
	cnclcd_isa_detach,
	cnclcd_isa_activate
};

int
cnclcd_isa_match(struct device *parent, void *match, void *aux)
{
	struct isa_attach_args *ia = aux;
	bus_space_handle_t ioh;
	bus_space_tag_t iot;
	int iobase;
	int i, k;

	iot = ia->ia_iot;
	iobase = ia->ia_iobase;

	if (bus_space_map(iot, iobase, COM_NPORTS, 0, &ioh))
		return (0);

	/* Force access to id register. */
	bus_space_write_1(iot, ioh, com_lcr, 0);
	bus_space_write_1(iot, ioh, com_iir, 0);
	for (i = 0; i < 32; i++) {
		k = bus_space_read_1(iot, ioh, com_iir);
		if (k & 0x38) {
			bus_space_read_1(iot, ioh, com_data); /* cleanup */
		} else {
			break;
		}
	}
	bus_space_unmap(iot, ioh, COM_NPORTS);
	if (i >= 32) {
		return (0);
	}
	ia->ia_iosize = COM_NPORTS;
	ia->ia_msize = 0;
	return (1);
}

void
cnclcd_isa_attach(struct device *parent, struct device *self, void *aux)
{
	struct cnclcd_softc *sc = (struct cnclcd_softc *)self;
	struct isa_attach_args *ia = aux;
	bus_space_handle_t ioh;
	bus_space_tag_t iot;
	int iobase;

	sc->sc_hwflags = 0;

	iobase = ia->ia_iobase;
	iot = ia->ia_iot;

	if (bus_space_map(iot, iobase, COM_NPORTS, 0, &ioh))
		panic("cnclcd_isa_attach: mapping failed");

	sc->sc_iot = iot;
	sc->sc_ioh = ioh;
	sc->sc_iobase = iobase;

	cnclcd_attach(sc);
}

int
cnclcd_isa_detach(struct device *self, int flags)
{
	return cnc_device_detach(self);
}

int
cnclcd_isa_activate(struct device *self, enum devact act)
{
	return cnc_device_activate(self);
}
