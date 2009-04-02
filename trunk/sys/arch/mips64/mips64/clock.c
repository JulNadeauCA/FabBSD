/*	$OpenBSD: clock.c,v 1.20 2008/04/07 22:36:26 miod Exp $ */

/*
 * Copyright (c) 2001-2004 Opsycon AB  (www.opsycon.se / www.opsycon.com)
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/evcount.h>
#include <sys/timetc.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <mips64/dev/clockvar.h>
#include <mips64/archtype.h>

static struct evcount clk_count;
static int clk_irq = 5;

int	clockmatch(struct device *, void *, void *);
void	clockattach(struct device *, struct device *, void *);

struct cfdriver clock_cd = {
	NULL, "clock", DV_DULL
};

struct cfattach clock_ca = {
	sizeof(struct device), clockmatch, clockattach
};

intrmask_t clock_int5(intrmask_t, struct trap_frame *);
void	clock_int5_init(void);

int	clock_started = 0;
u_int32_t cpu_counter_last;
u_int32_t cpu_counter_interval;
u_int32_t pendingticks;

u_int cp0_get_timecount(struct timecounter *);

struct timecounter cp0_timecounter = {
	cp0_get_timecount,	/* get_timecount */
	0,			/* no poll_pps */
	0xffffffff,		/* counter_mask */
	0,			/* frequency */
	"CP0",			/* name */
	0			/* quality */
};

#define	SECMIN	(60)		/* seconds per minute */
#define	SECHOUR	(60*SECMIN)	/* seconds per hour */

#define	YEARDAYS(year)	(((((year) + 1900) % 4) == 0 && \
			 ((((year) + 1900) % 100) != 0 || \
			  (((year) + 1900) % 400) == 0)) ? 366 : 365)

int
clockmatch(struct device *parent, void *vcf, void *aux)
{
	struct confargs *ca = aux;

	if (strcmp(ca->ca_name, clock_cd.cd_name) != 0)
		return 0;

	return 10;	/* Try to get clock early */
}

void
clockattach(struct device *parent, struct device *self, void *aux)
{
	printf(": ticker on int5 using count register\n");
	set_intr(INTPRI_CLOCK, CR_INT_5, clock_int5);
}

/*
 *	Clock interrupt code for machines using the on cpu chip
 *	counter register. This register counts at half the pipeline
 *	frequency so the frequency must be known and the options
 *	register wired to allow it's use.
 *
 *	The code is enabled by setting 'cpu_counter_interval'.
 */
void
clock_int5_init()
{
        int s;

	hz = 100;
	profhz = 100;
	stathz = 0;	/* XXX no stat clock yet */

	evcount_attach(&clk_count, "clock", (void *)&clk_irq, &evcount_intr);

        s = splclock();
        cpu_counter_interval = sys_config.cpu[0].clock / (hz * 2);
        cpu_counter_last = cp0_get_count() + cpu_counter_interval * 4;
        cp0_set_compare(cpu_counter_last);
        splx(s);
}

/*
 *  Interrupt handler for targets using the internal count register
 *  as interval clock. Normally the system is run with the clock
 *  interrupt always enabled. Masking is done here and if the clock
 *  can not be run the tick is just counted and handled later when
 *  the clock is unmasked again.
 */
intrmask_t
clock_int5(intrmask_t mask, struct trap_frame *tf)
{
	u_int32_t clkdiff;

	/*
	 * If clock is started count the tick, else just arm for a new.
	 */
	if (clock_started && cpu_counter_interval != 0) {
		clkdiff = cp0_get_count() - cpu_counter_last;
		while (clkdiff >= cpu_counter_interval) {
			cpu_counter_last += cpu_counter_interval;
			clkdiff = cp0_get_count() - cpu_counter_last;
			pendingticks++;
		}
		cpu_counter_last += cpu_counter_interval;
		pendingticks++;
	} else {
		cpu_counter_last = cpu_counter_interval + cp0_get_count();
	}

	cp0_set_compare(cpu_counter_last);
	/* Make sure that next clock tick has not passed */
	clkdiff = cp0_get_count() - cpu_counter_last;
	if (clkdiff > 0) {
		cpu_counter_last += cpu_counter_interval;
		pendingticks++;
		cp0_set_compare(cpu_counter_last);
	}

	if ((tf->cpl & SPL_CLOCKMASK) == 0) {
		while (pendingticks) {
			clk_count.ec_count++;
			hardclock(tf);
			pendingticks--;
		}
	}

	return CR_INT_5;	/* Clock is always on 5 */
}

/*
 * Wait "n" microseconds.
 */
void
delay(int n)
{
	int dly;
	int p, c;

	p = cp0_get_count();
	dly = (sys_config.cpu[0].clock / 1000000) * n / 2;
	while (dly > 0) {
		c = cp0_get_count();
		dly -= c - p;
		p = c;
	}
}

/*
 * Wait "n" nanoseconds.
 */
void
nanodelay(int n)
{
	int dly;
	int p, c;

	p = cp0_get_count();
	dly = ((sys_config.cpu[0].clock * n) / 1000000000) / 2;
	while (dly > 0) {
		c = cp0_get_count();
		dly -= c - p;
		p = c;
	}
}

/*
 *	Mips machine independent clock routines.
 */

struct tod_desc sys_tod;

/*
 * Start the real-time and statistics clocks. Leave stathz 0 since there
 * are no other timers available.
 */
void
cpu_initclocks()
{
	struct tod_desc *cd = &sys_tod;
	struct tod_time ct;
	u_int first_cp0, second_cp0, cycles_per_sec;
	int first_sec;

	/* Start the clock. */
	clock_int5_init();

	/*
	 * Calibrate the cycle counter frequency.
	 */
	if (cd->tod_get != NULL) {
		(*cd->tod_get)(cd->tod_cookie, 0, &ct);
		first_sec = ct.sec;

		/* Let the clock tick one second. */
		do {
			first_cp0 = cp0_get_count();
			(*cd->tod_get)(cd->tod_cookie, 0, &ct);
		} while (ct.sec == first_sec);
		first_sec = ct.sec;
		/* Let the clock tick one more second. */
		do {
			second_cp0 = cp0_get_count();
			(*cd->tod_get)(cd->tod_cookie, 0, &ct);
		} while (ct.sec == first_sec);

		cycles_per_sec = second_cp0 - first_cp0;
		sys_config.cpu[0].clock = cycles_per_sec * 2;
	}

	tick = 1000000 / hz;	/* number of micro-seconds between interrupts */
	tickadj = 240000 / (60 * hz);           /* can adjust 240ms in 60s */

	cp0_timecounter.tc_frequency = sys_config.cpu[0].clock / 2;
	tc_init(&cp0_timecounter);

	clock_started++;
}

/*
 * We assume newhz is either stathz or profhz, and that neither will
 * change after being set up above.  Could recalculate intervals here
 * but that would be a drag.
 */
void
setstatclockrate(newhz)
	int newhz;
{
}

/*
 * This code is defunct after 2099. Will Unix still be here then??
 */
static short dayyr[12] = {
	0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
};

/*
 * Initialize the time of day register, based on the time base which
 * is, e.g. from a filesystem.
 */
void
inittodr(time_t base)
{
	struct timespec ts;
	struct tod_time c;
	struct tod_desc *cd = &sys_tod;
	int days, yr;

	ts.tv_nsec = 0;

	if (base < 35 * SECYR) {
		printf("WARNING: preposterous time in file system");
		/* read the system clock anyway */
		base = 38 * SECYR;	/* 2008 */
	}

	/*
	 * Read RTC chip registers NOTE: Read routines are responsible
	 * for sanity checking clock. Dates after 19991231 should be
	 * returned as year >= 100.
	 */
	if (cd->tod_get) {
		(*cd->tod_get)(cd->tod_cookie, base, &c);
	} else {
		printf("WARNING: No TOD clock, believing file system.\n");
		goto bad;
	}

	days = 0;
	for (yr = 70; yr < c.year; yr++) {
		days += YEARDAYS(yr);
	}

	days += dayyr[c.mon - 1] + c.day - 1;
	if (YEARDAYS(c.year) == 366 && c.mon > 2) {
		days++;
	}

	/* now have days since Jan 1, 1970; the rest is easy... */
	ts.tv_sec = days * SECDAY + c.hour * 3600 + c.min * 60 + c.sec;
	tc_setclock(&ts);
	cd->tod_valid = 1;

	/*
	 * See if we gained/lost time.
	 */
	if (base < ts.tv_sec - 5*SECYR) {
		printf("WARNING: file system time much less than clock time\n");
	} else if (base > ts.tv_sec + 5*SECYR) {
		printf("WARNING: clock time much less than file system time\n");
		printf("WARNING: using file system time\n");
	} else {
		return;
	}

bad:
	ts.tv_sec = base;
	tc_setclock(&ts);
	cd->tod_valid = 1;
	printf("WARNING: CHECK AND RESET THE DATE!\n");
}

/*
 * Reset the TOD clock. This is done when the system is halted or
 * when the time is reset by the stime system call.
 */
void
resettodr()
{
	struct tod_time c;
	struct tod_desc *cd = &sys_tod;
	register int t, t2;

	/*
	 *  Don't reset TOD if time has not been set!
	 */
	if (!cd->tod_valid)
		return;

	/* compute the day of week. 1 is Sunday*/
	t2 = time_second / SECDAY;
	c.dow = (t2 + 5) % 7 + 1;	/* 1/1/1970 was thursday */

	/* compute the year */
	t2 = time_second / SECDAY;
	c.year = 69;
	while (t2 >= 0) {	/* whittle off years */
		t = t2;
		c.year++;
		t2 -= YEARDAYS(c.year);
	}

	/* t = month + day; separate */
	t2 = YEARDAYS(c.year);
	for (c.mon = 1; c.mon < 12; c.mon++) {
		if (t < dayyr[c.mon] + (t2 == 366 && c.mon > 1))
			break;
	}

	c.day = t - dayyr[c.mon - 1] + 1;
	if (t2 == 366 && c.mon > 2) {
		c.day--;
	}

	t = time_second % SECDAY;
	c.hour = t / 3600;
	t %= 3600;
	c.min = t / 60;
	c.sec = t % 60;

	if (cd->tod_set)
		(*cd->tod_set)(cd->tod_cookie, &c);
}

u_int
cp0_get_timecount(struct timecounter *tc)
{
	return (cp0_get_count());
}