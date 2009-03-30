/*	Public domain	*/

#ifndef _SYS_CNC_H_
#define _SYS_CNC_H_

#include <sys/stdint.h>

#define CNC_BUF_SIZE		1024		/* size of instruction buffer */
#define CNC_MAX_AXES		3
#define CNC_NAXES		CNC_MAX_AXES

#define CNC_MAX_SERVOS		3
#define CNC_MAX_SPINDLES	2
#define CNC_MAX_ESTOPS		13
#define CNC_MAX_ENCODERS	4
#define CNC_MAX_MPGS		2
#define CNC_MAX_LCDS		4

/* Allowed position error (steps) */
#define CNC_POS_ERROR		1e-6

enum cnc_axis {
	CNC_X = 0,
	CNC_Y = 1,
	CNC_Z = 2,
	CNC_U = 3,
	CNC_V = 4,
	CNC_W = 5,
	CNC_I = 6,
	CNC_J = 7,
	CNC_K = 8,
	CNC_A = 9,
	CNC_B = 10,
	CNC_C = 11,
	CNC_AXIS_LAST
};

typedef uint64_t cnc_step_t;
typedef int64_t cnc_pos_t;
typedef int64_t cnc_time_t;
typedef int64_t cnc_utime_t;

/* Position vector */
typedef struct cnc_vector {
	cnc_pos_t v[CNC_NAXES];
} cnc_vec_t;

/* Velocity parameters - arguments to cncmove(2) */
struct cnc_velocity {
	u_long v0;		 /* min steps/s */
	u_long F;		 /* max steps/s */
	u_long Amax;		 /* max steps/ms^2 */
	u_long Jmax;		 /* max steps/ms^3 */
};

/* Spindle control - arguments to spinctl(2) */
enum cnc_spindle_op {
	CNC_SPINDLE_GET_DIR,		/* sense current direction */
	CNC_SPINDLE_SET_DIR,		/* change current direction */
	CNC_SPINDLE_GET_SPEED,		/* sense revolutions per minute */
	CNC_SPINDLE_SET_SPEED,		/* set revolutions per minute */
	CNC_SPINDLE_START,		/* accelerate spindle to speed */
	CNC_SPINDLE_STOP		/* decelerate spindle */
};
struct cnc_spindle_args {
	int dir;			/* 1=CW, -1=CCW */
	u_int speed;			/* speed in RPM */
};

/* Toolchanger control - arguments to atcctl(2) */
enum cnc_atc_op {
	CNC_ATC_PREPARE,		/* move a tool to the ready position */
	CNC_ATC_CHANGE,			/* change tool of specified spindle */
};
struct cnc_atc_args {
	int tool;			/* tool index */
	int spindle;			/* target spindle */
};

/* Laser beam control - arguments to laserctl(2) */
enum cnc_laser_op {
	CNC_LASER_ON,			/* TTL trigger high */
	CNC_LASER_OFF,			/* TTL trigger low */
	CNC_LASER_GET_CURRENT,		/* sense supplied current */
	CNC_LASER_SET_CURRENT,		/* set steady-state current */
};
struct cnc_laser_args {
	int current;			/* target current (mA) */
};

/* Pick and place control - arguments to pickplacectl(2) */
enum cnc_pickplace_op {
	CNC_PICKPLACE_PREPARE,		/* set reel in pickup position */
	CNC_PICKPLACE_PICK,		/* pick action */
	CNC_PICKPLACE_RELEASE		/* release action */
};
struct cnc_pickplace_args {
	int reel;			/* reel index */
};

/* Coolant system control - arguments to coolantctl(2) */
enum cnc_coolant_op {
	CNC_COOLANT_MISTER,		/* toggle mister */
	CNC_COOLANT_FLOOD,		/* toggle flood coolant */
	CNC_COOLANT_VORTEX		/* toggle vortex tube */
};

/* Mode of interpolation between coordinates */
enum cnc_interp_mode {
	CNC_INTERP_LINEAR,		/* linear interpolation */
	CNC_INTERP_LAST
};

/*
 * Kinematic limits. These values must be adjusted based on the physical
 * limits of the motors and of the load they are driving.
 */
struct cnc_kinlimits {
	cnc_utime_t Fmax;		/* Maximum velocity (steps/sec) */
	cnc_utime_t Amax;		/* Maximum acceleration (steps/ms^2) */
	cnc_utime_t Jmax;		/* Maximum jerk (steps/ms^3) */
};

/* Calibrated delay loop timings. */
struct cnc_timings {
	cnc_utime_t hz;		/* 1Hz reference */
	cnc_utime_t move_jog;	/* Execution time of cnc_move_jog() */
};

/* For CNC_GETDEVICEINFO */
struct cnc_device_info {
	int nservos;		/* servo(4) devices */
	int nspindles;		/* spindle(4) devices */
	int nestops;		/* estop(4) devices */
	int nencoders;		/* encoder(4) devices */
	int nmpgs;		/* mpg(4) devices */
};

#define CNC_GETPOS		_IOR('C', 0, struct cnc_vector)
#define CNC_SETPOS		_IOWR('C', 1, struct cnc_vector)
#define CNC_GETDEVICEINFO	_IOR('C', 2, struct cnc_device_info)
#define CNC_GETKINLIMITS	_IOR('C', 3, struct cnc_kinlimits)
#define CNC_SETKINLIMITS	_IOWR('C', 4, struct cnc_kinlimits)
#define CNC_GETTIMINGS		_IOR('C', 5, struct cnc_timings)
#define CNC_SETTIMINGS		_IOWR('C', 6, struct cnc_timings)
#define CNC_CALTIMINGS		_IOWR('C', 7, struct cnc_timings)

#if !defined(_KERNEL)

#include <sys/cdefs.h>

__BEGIN_DECLS
int	cncmove(const struct cnc_velocity *, const cnc_vec_t *tgt);
int	cncjog(const struct cnc_velocity *);
int	cncjogstep(const struct cnc_velocity *);
int	spinctl(int, enum cnc_spindle_op, const struct cnc_spindle_args *);
int	atcctl(int, enum cnc_atc_op, const struct cnc_atc_args *);
int	laserctl(int, enum cnc_laser_op, const struct cnc_laser_args *);
int	pickplacectl(int, enum cnc_pickplace_op, const struct cnc_pickplace_args *);
int	coolantctl(int, enum cnc_coolant_op, const struct cnc_coolant_args *);
int	estop(void);
__END_DECLS

#endif /* !_KERNEL */

#endif	/* !_SYS_CNC_H_ */
