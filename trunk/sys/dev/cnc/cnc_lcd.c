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

/*
 * 16450/16550 UART driver modified for text-based LCD output and polled-mode
 * operation. This allows a standard serial link to be used in interfacing
 * with common serial LCD control ICs such as the EDE700 or EDE702.
 *
 * Polled-mode operation is useful for debugging, and is the cnc(4) subsystem's
 * only way of displaying information during program execution in real time.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/cnc.h>

#include <machine/bus.h>

#include <dev/ic/comreg.h>
#include <dev/ic/ns16550reg.h>
#define	com_lcr	com_cfcr

#include <dev/cnc/cncvar.h>
#include <dev/cnc/cnc_devicevar.h>
#include <dev/cnc/cnc_lcdvar.h>

struct cfdriver cnclcd_cd = {
	NULL, "cnclcd", DV_DULL
};

void	cnclcd_attach_fifo(struct cnclcd_softc *);

void
cnclcd_attach(struct cnclcd_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	/* Generic CNC device initialization. */
	if (cnc_device_attach(sc, CNC_DEVICE_LCD) == -1)
		return;

	sc->sc_open = 0;
	sc->sc_flags = 0;

	/* Disable interrupts */
	bus_space_write_1(iot, ioh, com_ier, 0);

	/*
	 * Probe for all known forms of UART.
	 */
	bus_space_write_1(iot, ioh, com_lcr, LCR_EFR);
	bus_space_write_1(iot, ioh, com_efr, 0);
	bus_space_write_1(iot, ioh, com_lcr, 0);
	bus_space_write_1(iot, ioh, com_fifo, FIFO_ENABLE);
	delay(100);

	/*
	 * Skip specific probes if attachment code knows it already.
	 */
	switch (bus_space_read_1(iot, ioh, com_iir) >> 6) {
	case 0:
		sc->sc_uarttype = COM_UART_16450;
		break;
	case 2:
		sc->sc_uarttype = COM_UART_16550;
		break;
	case 3:
		sc->sc_uarttype = COM_UART_16550A;
		break;
	default:
		sc->sc_uarttype = COM_UART_UNKNOWN;
		break;
	}

	/* Read the LCR */
	sc->sc_lcr = bus_space_read_1(iot, ioh, com_lcr);

	if (sc->sc_uarttype == COM_UART_16550A) { /* Probe for ST16650s */
		bus_space_write_1(iot, ioh, com_lcr, sc->sc_lcr|LCR_DLAB);
		if (bus_space_read_1(iot, ioh, com_efr) == 0) {
			sc->sc_uarttype = COM_UART_ST16650;
		} else {
			bus_space_write_1(iot, ioh, com_lcr, LCR_EFR);
			if (bus_space_read_1(iot, ioh, com_efr) == 0)
				sc->sc_uarttype = COM_UART_ST16650V2;
		}
	}
	if (sc->sc_uarttype == COM_UART_16550A) { /* Probe for TI16750s */
		bus_space_write_1(iot, ioh, com_lcr, sc->sc_lcr|LCR_DLAB);
		bus_space_write_1(iot, ioh, com_fifo,
		    FIFO_ENABLE | FIFO_ENABLE_64BYTE);
		if ((bus_space_read_1(iot, ioh, com_iir) >> 5) == 7) {
#if 0
			bus_space_write_1(iot, ioh, com_lcr, 0);
			if ((bus_space_read_1(iot, ioh, com_iir) >> 5) == 6)
#endif
				sc->sc_uarttype = COM_UART_TI16750;
		}
		bus_space_write_1(iot, ioh, com_fifo, FIFO_ENABLE);
	}

	/* Reset the LCR (latch access is probably enabled). */
	bus_space_write_1(iot, ioh, com_lcr, sc->sc_lcr);

	if (sc->sc_uarttype == COM_UART_16450) { /* Probe for 8250 */
		u_int8_t scr0, scr1, scr2;

		scr0 = bus_space_read_1(iot, ioh, com_scratch);
		bus_space_write_1(iot, ioh, com_scratch, 0xa5);
		scr1 = bus_space_read_1(iot, ioh, com_scratch);
		bus_space_write_1(iot, ioh, com_scratch, 0x5a);
		scr2 = bus_space_read_1(iot, ioh, com_scratch);
		bus_space_write_1(iot, ioh, com_scratch, scr0);

		if ((scr1 != 0xa5) || (scr2 != 0x5a))
			sc->sc_uarttype = COM_UART_8250;
	}

	/*
	 * Print UART type and initialize ourself.
	 */
	sc->sc_fifolen = 0;
	switch (sc->sc_uarttype) {
	case COM_UART_UNKNOWN:
		printf(": unknown uart\n");
		break;
	case COM_UART_8250:
		printf(": ns8250, no fifo\n");
		break;
	case COM_UART_16450:
		printf(": ns16450, no fifo\n");
		break;
	case COM_UART_16550:
		printf(": ns16550, no working fifo\n");
		break;
	case COM_UART_16550A:
		sc->sc_fifolen = 16;
		printf(": ns16550a, %d byte fifo\n", sc->sc_fifolen);
		SET(sc->sc_hwflags, COM_HW_FIFO);
		break;
	case COM_UART_ST16650:
		printf(": st16650, no working fifo\n");
		break;
	case COM_UART_ST16650V2:
		sc->sc_fifolen = 32;
		printf(": st16650, %d byte fifo\n", sc->sc_fifolen);
		SET(sc->sc_hwflags, COM_HW_FIFO);
		break;
	case COM_UART_ST16C654:
		printf(": st16c654, 64 byte fifo\n");
		SET(sc->sc_hwflags, COM_HW_FIFO);
		sc->sc_fifolen = 64;
		break;
	case COM_UART_TI16750:
		printf(": ti16750, 64 byte fifo\n");
		SET(sc->sc_hwflags, COM_HW_FIFO);
		sc->sc_fifolen = 64;
		break;
	default:
		panic("cnclcd_attach: bad fifo type");
	}

	if (ISSET(sc->sc_hwflags, COM_HW_FIFO)) {
		cnclcd_attach_fifo(sc);
	}
	if (sc->sc_fifolen == 0) {
		CLR(sc->sc_hwflags, COM_HW_FIFO);
		sc->sc_fifolen = 1;
	}

	/* clear and disable fifo */
	bus_space_write_1(iot, ioh, com_fifo, FIFO_RCV_RST | FIFO_XMT_RST);
	(void)bus_space_read_1(iot, ioh, com_data);
	bus_space_write_1(iot, ioh, com_fifo, 0);

	sc->sc_msr = 0;
	sc->sc_mcr = bus_space_read_1(iot, ioh, com_mcr);
}

void
cnclcd_attach_fifo(struct cnclcd_softc *sc)
{
	bus_space_handle_t ioh = sc->sc_ioh;
	bus_space_tag_t iot = sc->sc_iot;
	u_int8_t fifo;
	int timo, len;

	bus_space_write_1(iot, ioh, com_ier, 0);
	bus_space_write_1(iot, ioh, com_lcr, LCR_DLAB);
	bus_space_write_1(iot, ioh, com_dlbl, 3);
	bus_space_write_1(iot, ioh, com_dlbh, 0);
	bus_space_write_1(iot, ioh, com_lcr, LCR_PNONE | LCR_8BITS);
	bus_space_write_1(iot, ioh, com_mcr, MCR_LOOPBACK);

	fifo = FIFO_ENABLE | FIFO_RCV_RST | FIFO_XMT_RST;
	if (sc->sc_uarttype == COM_UART_TI16750)
		fifo |= FIFO_ENABLE_64BYTE;

	bus_space_write_1(iot, ioh, com_fifo, fifo);

	for (len = 0; len < 256; len++) {
		bus_space_write_1(iot, ioh, com_data, (len + 1));
		timo = 2000;
		while (!ISSET(bus_space_read_1(iot, ioh, com_lsr),
		    LSR_TXRDY) && --timo)
			delay(1);
		if (!timo)
			break;
	}

	delay(100);

	for (len = 0; len < 256; len++) {
		timo = 2000;
		while (!ISSET(bus_space_read_1(iot,ioh,com_lsr),LSR_RXRDY) &&
		    --timo) {
			delay(1);
		}
		if (!timo || bus_space_read_1(iot, ioh, com_data) != (len + 1))
			break;
	}

	/* For safety, always use the smaller value. */
	if (sc->sc_fifolen > len) {
		sc->sc_fifolen = len;
	}
	printf("%s: probed fifo depth: %d bytes\n",
	    ((struct device *)sc)->dv_xname, len);
}

int
cnclcd_speed(long freq, long speed)
{
#define	divrnd(n, q)	(((n)*2/(q)+1)/2)	/* divide and round off */

	int x, err;

	if (speed == 0) { return (0); }
	if (speed < 0) { return (-1); }
	x = divrnd((freq / 16), speed);
	if (x <= 0) { return (-1); }
	err = divrnd((quad_t)freq * 1000 / 16, speed * x) - 1000;
	if (err < 0) { err = -err; }
	if (err > COM_TOLERANCE) { return (-1); }
	return (x);
#undef	divrnd
}

int
cnclcd_open(struct cnclcd_softc *sc, int speed, int bits,
    enum cnclcd_parity parity, int stopbits)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int s, rate;

	if (sc->sc_open)
		return (-1);

	s = spltty();
	sc->sc_speed = speed;

	/* Clear DTR */
	CLR(sc->sc_mcr, MCR_DTR);
	bus_space_write_1(iot, ioh, com_mcr, sc->sc_mcr);

	/* Configure bits */
	CLR(sc->sc_lcr, LCR_5BITS);
	CLR(sc->sc_lcr, LCR_6BITS);
	CLR(sc->sc_lcr, LCR_7BITS);
	CLR(sc->sc_lcr, LCR_8BITS);
	switch (bits) {
	case 5:		SET(sc->sc_lcr, LCR_5BITS);	break;
	case 6:		SET(sc->sc_lcr, LCR_6BITS);	break;
	case 7:		SET(sc->sc_lcr, LCR_7BITS);	break;
	case 8:		SET(sc->sc_lcr, LCR_8BITS);	break;
	default:					break;
	}

	/* Configure parity and stop bits */
	if (parity != CNCLCD_PNONE) {
		SET(sc->sc_lcr, LCR_PENAB);
		switch (parity) {
		case CNCLCD_PZERO:	SET(sc->sc_lcr, LCR_PZERO);	break;
		case CNCLCD_PONE:	SET(sc->sc_lcr, LCR_PONE);	break;
		case CNCLCD_PEVEN:	SET(sc->sc_lcr, LCR_PEVEN);	break;
		case CNCLCD_PODD:	SET(sc->sc_lcr, LCR_PODD);	break;
		default:		panic("cnclcd_open: parity");
		}
	} else {
		CLR(sc->sc_lcr, LCR_PENAB);
		CLR(sc->sc_lcr, LCR_PZERO);
		CLR(sc->sc_lcr, LCR_PONE);
		CLR(sc->sc_lcr, LCR_PEVEN);
		CLR(sc->sc_lcr, LCR_PODD);
	}
	if (stopbits == 2) {
		SET(sc->sc_lcr, LCR_STOPB);
	} else {
		CLR(sc->sc_lcr, LCR_STOPB);
	}
	bus_space_write_1(iot, ioh, com_lcr, sc->sc_lcr);

	/* Configure speed */
	bus_space_write_1(iot, ioh, com_lcr, sc->sc_lcr|LCR_DLAB);
	rate = cnclcd_speed(COM_FREQ, sc->sc_speed);
	bus_space_write_1(iot, ioh, com_dlbl, rate);
	bus_space_write_1(iot, ioh, com_dlbh, rate >> 8);
	bus_space_write_1(iot, ioh, com_lcr, sc->sc_lcr);

	/* Set the FIFO threshold based on the speed. */
	if (ISSET(sc->sc_hwflags, COM_HW_FIFO)) {
		if (sc->sc_uarttype == COM_UART_TI16750) {
			bus_space_write_1(iot, ioh, com_lcr, sc->sc_lcr|LCR_DLAB);
			bus_space_write_1(iot, ioh, com_fifo,
			    FIFO_ENABLE | FIFO_ENABLE_64BYTE |
			    (sc->sc_speed <= 1200 ? FIFO_TRIGGER_1 : FIFO_TRIGGER_8));
			bus_space_write_1(iot, ioh, com_lcr, sc->sc_lcr);
		} else {
			bus_space_write_1(iot, ioh, com_fifo,
			    FIFO_ENABLE |
			    (sc->sc_speed <= 1200 ? FIFO_TRIGGER_1 : FIFO_TRIGGER_8));
		}
	} else {
		bus_space_write_1(iot, ioh, com_lcr, sc->sc_lcr);
	}

	/*
	 * Wake up the sleepy heads.
	 */
	switch (sc->sc_uarttype) {
	case COM_UART_ST16650:
	case COM_UART_ST16650V2:
		bus_space_write_1(iot, ioh, com_lcr, LCR_EFR);
		bus_space_write_1(iot, ioh, com_efr, EFR_ECB);
		bus_space_write_1(iot, ioh, com_ier, 0);
		bus_space_write_1(iot, ioh, com_efr, 0);
		bus_space_write_1(iot, ioh, com_lcr, 0);
		break;
	case COM_UART_TI16750:
		bus_space_write_1(iot, ioh, com_ier, 0);
		break;
	}

	if (ISSET(sc->sc_hwflags, COM_HW_FIFO)) {
		u_int8_t fifo = FIFO_ENABLE|FIFO_RCV_RST|FIFO_XMT_RST;

		if (sc->sc_speed <= 1200) {
			fifo |= FIFO_TRIGGER_1;
		} else {
			fifo |= FIFO_TRIGGER_8;
		}
		if (sc->sc_uarttype == COM_UART_TI16750) {
			fifo |= FIFO_ENABLE_64BYTE;
			bus_space_write_1(iot, ioh, com_lcr, sc->sc_lcr|LCR_DLAB);
		}

		/*
		 * (Re)enable and drain FIFOs.
		 *
		 * Certain SMC chips cause problems if the FIFOs are
		 * enabled while input is ready. Turn off the FIFO
		 * if necessary to clear the input. Test the input
		 * ready bit after enabling the FIFOs to handle races
		 * between enabling and fresh input.
		 *
		 * Set the FIFO threshold based on the receive speed.
		 */
		for (;;) {
			bus_space_write_1(iot, ioh, com_fifo, 0);
			delay(100);
			(void)bus_space_read_1(iot, ioh, com_data);
			bus_space_write_1(iot, ioh, com_fifo,
			    fifo | FIFO_RCV_RST | FIFO_XMT_RST);
			delay(100);
			if (!ISSET(bus_space_read_1(iot,ioh,com_lsr),LSR_RXRDY))
				break;
		}
		if (sc->sc_uarttype == COM_UART_TI16750)
			bus_space_write_1(iot, ioh, com_lcr, sc->sc_lcr);
	}

	/* Flush any pending I/O. */
	while (ISSET(bus_space_read_1(iot, ioh, com_lsr), LSR_RXRDY))
		(void)bus_space_read_1(iot, ioh, com_data);

	/* Set DTR|RTS */
	SET(sc->sc_mcr, MCR_DTR|MCR_RTS);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, com_mcr, sc->sc_mcr);

	/* Read initial MSR */
	sc->sc_msr = bus_space_read_1(iot, ioh, com_msr);

	sc->sc_open = 1;
	splx(s);
	return (0);
}

int
cnclcd_close(struct cnclcd_softc *sc)
{
	int s;

	s = spltty();
	if (sc->sc_open) {
		CLR(sc->sc_mcr, MCR_DTR|MCR_RTS);
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, com_mcr, sc->sc_mcr);
		sc->sc_open = 0;
	}
	splx(s);
	return (0);
}

void
cnclcd_puts(struct cnclcd_softc *sc, const char *s)
{
	const char *c;
	
	for (c = &s[0]; *c != '\0'; c++)
		cnclcd_putc(sc, *c);
}

void
cnclcd_putc(struct cnclcd_softc *sc, int c)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int timo;

	/* Wait for any pending transmission to finish. */
	timo = 2000;
	while (!ISSET(bus_space_read_1(iot,ioh,com_lsr),LSR_TXRDY) && --timo)
		delay(1);

	bus_space_write_1(iot, ioh, com_data, (u_int8_t)(c & 0xff));
	bus_space_barrier(iot, ioh, 0, COM_NPORTS,
	    (BUS_SPACE_BARRIER_READ|BUS_SPACE_BARRIER_WRITE));

	/* Wait for this transmission to complete. */
	timo = 2000;
	while (!ISSET(bus_space_read_1(iot,ioh,com_lsr),LSR_TXRDY) && --timo)
		delay(1);
}

#if 0
void
cnclcd_init(bus_space_tag_t iot, bus_space_handle_t ioh, int rate, int frequency)
{
	int s;

	s = splhigh();
	bus_space_write_1(iot, ioh, com_lcr, LCR_DLAB);
	rate = cnclcd_speed(frequency, rate); /* XXX not cnclcd_default_rate? */
	bus_space_write_1(iot, ioh, com_dlbl, rate);
	bus_space_write_1(iot, ioh, com_dlbh, rate >> 8);
	bus_space_write_1(iot, ioh, com_lcr, LCR_8BITS);
	bus_space_write_1(iot, ioh, com_mcr, MCR_DTR | MCR_RTS);
	bus_space_write_1(iot, ioh, com_ier, 0);  /* Make sure they are off */
	bus_space_write_1(iot, ioh, com_fifo,
	    FIFO_ENABLE | FIFO_RCV_RST | FIFO_XMT_RST | FIFO_TRIGGER_1);
	splx(s);
}
#endif
