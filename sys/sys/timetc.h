/*-
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $OpenBSD: timetc.h,v 1.2 2006/10/30 20:19:33 otto Exp $
 * $FreeBSD: src/sys/sys/timetc.h,v 1.57 2003/04/10 23:07:24 des Exp $
 */

#ifndef _SYS_TIMETC_H_
#define	_SYS_TIMETC_H_

#ifndef _KERNEL
#error "no user-serviceable parts inside"
#endif

/*-
 * `struct timecounter' is the interface between the hardware which implements
 * a timecounter and the MI code which uses this to keep track of time.
 *
 * A timecounter is a binary counter which has two properties:
 *    * it runs at a fixed, known frequency.
 *    * it has sufficient bits to not roll over in less than approximately
 *      max(2 msec, 2/HZ seconds).  (The value 2 here is really 1 + delta,
 *      for some indeterminate value of delta.)
 */

struct timecounter;
typedef u_int timecounter_get_t(struct timecounter *);
typedef void timecounter_pps_t(struct timecounter *);

struct timecounter {
	timecounter_get_t	*tc_get_timecount;
		/*
		 * This function reads the counter.  It is not required to
		 * mask any unimplemented bits out, as long as they are
		 * constant.
		 */
	timecounter_pps_t	*tc_poll_pps;
		/*
		 * This function is optional.  It will be called whenever the
		 * timecounter is rewound, and is intended to check for PPS
		 * events.  Normal hardware does not need it but timecounters
		 * which latch PPS in hardware (like sys/pci/xrpu.c) do.
		 */
	u_int 			tc_counter_mask;
		/* This mask should mask off any unimplemented bits. */
	u_int64_t		tc_frequency;
		/* Frequency of the counter in Hz. */
	char			*tc_name;
		/* Name of the timecounter. */
	int			tc_quality;
		/*
		 * Used to determine if this timecounter is better than
		 * another timecounter higher means better.  Negative
		 * means "only use at explicit request".
		 */
	void			*tc_priv;
		/* Pointer to the timecounter's private parts. */
	struct timecounter	*tc_next;
		/* Pointer to the next timecounter. */
	int64_t			tc_freq_adj;
		/* Current frequency adjustment. */
};

extern struct timecounter *timecounter;

u_int64_t tc_getfrequency(void);
void	tc_init(struct timecounter *tc);
void	tc_setclock(struct timespec *ts);
void	tc_ticktock(void);
void	inittimecounter(void);
int	sysctl_tc(int *, u_int, void *, size_t *, void *, size_t);
int	tc_adjfreq(int64_t *, int64_t *);

#endif /* !_SYS_TIMETC_H_ */