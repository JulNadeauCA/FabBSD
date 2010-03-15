/*	Public domain	*/

#include "cnc_math.h"
#include "cnc_quintic.h"

extern const char *cnc_axis_names[];
extern struct cnc_kinlimits cnc_kinlimits;

extern struct servo_softc      *cnc_servos[CNC_NAXES];
extern struct spindle_softc    *cnc_spindles[CNC_MAX_SPINDLES];
extern struct estop_softc      *cnc_estops[CNC_MAX_ESTOPS];
extern struct encoder_softc    *cnc_encoders[CNC_MAX_ENCODERS];
extern struct mpg_softc        *cnc_mpgs[CNC_MAX_MPGS];
extern struct cnclcd_softc     *cnc_lcds[CNC_MAX_LCDS];
extern struct cncstatled_softc *cnc_status_led;

extern int cnc_simulate;
extern int cnc_capture;
extern int cnc_nservos;
extern int cnc_nspindles;
extern int cnc_nestops;
extern int cnc_nencoders;
extern int cnc_nmpgs;
extern int cnc_nlcds;

extern struct cnc_vector cnc_pos;
extern struct cnc_timings cnc_timings;

int  cncopen(dev_t, int, int, struct proc *);
int  cncclose(dev_t, int, int, struct proc *);
int  cncioctl(dev_t, u_long, caddr_t, int, struct proc *);
int  cncwrite(dev_t, struct uio *, int);
void cncattach(int);
int  cncdetach(struct device *, int);
int  cncactivate(struct device *, enum devact);

void cnc_message(const char *);
int  cnc_estop_raised(void);
void cnc_inc_axis(enum cnc_axis, int);
void cnc_clip_velocity(const struct cnc_quintic_profile *, cnc_real_t *);

cnc_utime_t cnc_calibrate_hz(void);
cnc_utime_t cnc_calibrate_move_jog(void);

static __inline__ int
cnc_calibrated(void)
{
	if (cnc_timings.hz == 0 || cnc_timings.move_jog == 0) {
		printf("cnc: timings are not calibrated!\n");
		return (-1);
	}
	return (0);
}
