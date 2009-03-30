/*
 * Copyright (c) 1997 - 1998, Jason Downs.  All rights reserved.
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
/*
 * Copyright (c) 1996 Christopher G. Demetriou.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

enum cnclcd_parity {
	CNCLCD_PZERO,		/* Space */
	CNCLCD_PONE,		/* Mark */
	CNCLCD_PEVEN,		/* Even */
	CNCLCD_PODD,		/* Odd */
	CNCLCD_PNONE		/* No parity */
};

#define	CNC_LCD_IBUFSIZE	(2 * 512)
#define	CNC_LCD_IHIGHWATER	((3 * CNC_LCD_IBUFSIZE) / 4)

struct cnclcd_softc {
	struct cnc_device sc_cdev;
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
	bus_addr_t sc_iobase;		/* For bus-specific layer */
	
	u_char sc_flags;
#define CNCLCD_USE_CRTSCTS	0x01	/* Use CRTSCTS */

	int sc_open;			/* UART is initialized */
	int sc_speed;			/* Baud rate */

	u_char sc_uarttype;
#define COM_UART_UNKNOWN	0x00		/* unknown */
#define COM_UART_8250		0x01		/* no fifo */
#define COM_UART_16450		0x02		/* no fifo */
#define COM_UART_16550		0x03		/* no working fifo */
#define COM_UART_16550A		0x04		/* 16 byte fifo */
#define COM_UART_ST16650	0x05		/* no working fifo */
#define COM_UART_ST16650V2	0x06		/* 32 byte fifo */
#define COM_UART_TI16750	0x07		/* 64 byte fifo */
#define	COM_UART_ST16C654	0x08		/* 64 bytes fifo */
#define	COM_UART_XR16850	0x10		/* 128 byte fifo */
#define COM_UART_PXA2X0		0x11		/* 16 byte fifo */
#define	COM_UART_OX16C950	0x12		/* 128 byte fifo */

	u_char sc_hwflags;
#define	COM_HW_FIFO	0x02
#define	COM_HW_SIR	0x20

	int    sc_fifolen;			/* FIFO length in bytes */
	u_char sc_msr;				/* MODEM Status Register */
	u_char sc_mcr;				/* MODEM Control Register */
	u_char sc_lcr;				/* Line Control Register */
};

void	cnclcd_attach(struct cnclcd_softc *);
int	cnclcd_activate(struct device *, enum devact);

int	cnclcd_speed(long, long);
void	cnclcd_start(struct cnclcd_softc *);
void	cnclcd_puts(struct cnclcd_softc *, const char *);
void	cnclcd_putc(struct cnclcd_softc *, int);
void	cnclcd_set_dtr_rts(void *, int);

int	cnclcd_open(struct cnclcd_softc *, int, int, enum cnclcd_parity, int);
int	cnclcd_close(struct cnclcd_softc *);
