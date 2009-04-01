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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/bus.h>

#include <dev/pci/pucvar.h>
#include <dev/ic/lptgpiovar.h>

int	lptgpio_puc_probe(struct device *, void *, void *);
void	lptgpio_puc_attach(struct device *, struct device *, void *);
int	lptgpio_puc_detach(struct device *, int);

struct cfattach lptgpio_puc_ca = {
	sizeof(struct lptgpio_softc), lptgpio_puc_probe, lptgpio_puc_attach
};

int
lptgpio_puc_probe(struct device *parent, void *match, void *aux)
	struct device *parent;
	void *match, *aux;
{
	struct puc_attach_args *aa = aux;

	return (aa->type != PUC_PORT_TYPE_LPT) ? 0 : 1;
}

void
lptgpio_puc_attach(struct device *parent, struct device *self, void *aux)
{
	struct lptgpio_softc *sc = (void *)self;
	struct puc_attach_args *aa = aux;

	sc->sc_iot = aa->t;
	sc->sc_ioh = aa->h;

	lptgpio_attach_common(sc);
}
