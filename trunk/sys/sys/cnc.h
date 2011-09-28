/*	Public domain	*/

#ifndef _SYS_CNC_H_
#define _SYS_CNC_H_

#include <sys/stdint.h>

#define CNC_BUF_SIZE		1024		/* size of instruction buffer */
#define CNC_MAX_AXES		3
#define CNC_MAX_TOOLS		64
#define CNC_NAXES		CNC_MAX_AXES

#define CNC_MAX_SERVOS		3
#define CNC_MAX_SPINDLES	2
#define CNC_MAX_ATCS		2
#define CNC_MAX_ESTOPS		13
#define CNC_MAX_ENCODERS	4
#define CNC_MAX_MPGS		2
#define CNC_MAX_LCDS		4
#define CNC_MAX_SPOTWELDERS	4

/* Allowed position error (steps) */
#define CNC_POS_ERROR		1e-6

/* Math constants */
#define	CNC_PI		3.14159265358979323846	/* pi */
#define	CNC_PI_2	1.57079632679489661923	/* pi/2 */
#define	CNC_PI_P2	(CNC_PI*CNC_PI)		/* pi^2 */

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

typedef float cnc_real_t;
typedef uint64_t cnc_step_t;
typedef int64_t cnc_pos_t;
typedef int64_t cnc_time_t;
typedef int64_t cnc_utime_t;
typedef cnc_pos_t cnc_dist_t;
typedef u_long cnc_vel_t;

/* Position vector */
typedef struct cnc_vector {
	cnc_pos_t v[CNC_NAXES];
} cnc_vec_t;

/* Velocity parameters - arguments to cncmove(2) */
struct cnc_velocity {
	cnc_vel_t v0;		 /* min steps/s */
	cnc_vel_t F;		 /* max steps/s */
	cnc_vel_t Amax;		 /* max steps/us^2 */
	cnc_vel_t Jmax;		 /* max steps/us^3 */
};
#define CNC_VELOCITY_INITIALIZER(v0,F,Amax,Jmax) \
	{ (v0), (F), (Amax), (Jmax) }

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
	CNC_ATC_GET_NSLOTS,		/* return total number of slots */
	CNC_ATC_GET_ACTIVE_TOOL,	/* return # of active tool */
	CNC_ATC_GET_READY_TOOL,		/* return # of tool in ready position */
	CNC_ATC_PREPARE,		/* move a tool to the ready position */
	CNC_ATC_CHANGE,			/* change tool in target spindle */
};
struct cnc_atc_args {
	int nslots;			/* for GET_NSLOTS */
	int slot;			/* for GET_SLOT */
	int tool;			/* tool for PREPARE, CHANGE */
	int spindle;			/* target spindle for CHANGE */
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
};

/* For CNC_GETDEVICEINFO */
struct cnc_device_info {
	int nservos;		/* servo(4) devices */
	int nspindles;		/* spindle(4) devices */
	int natcs;		/* atc(4) devices */
	int nestops;		/* estop(4) devices */
	int nencoders;		/* encoder(4) devices */
	int nmpgs;		/* mpg(4) devices */
};

/*
 * General tool class.
 * Sync with libcnc cnc_tool_strings[].
 */
enum cnc_tool_type {
	CNC_TOOL_NONE,
	CNC_TOOL_DRILL,			/* Twist drill or countersink */
	CNC_TOOL_HOLESAW,		/* Hole saw */
	CNC_TOOL_ENDMILL,		/* Endmill or special milling cutter */
	CNC_TOOL_SLITSAW,		/* Slitting saw */
	CNC_TOOL_FACEMILL,		/* Face mill / fly cutter */
	CNC_TOOL_GEARCUTTER,		/* Involute gear cutter */
	CNC_TOOL_LAST
};

/*
 * Milling cutter shape subtype
 * Sync with libcnc cnc_endmill_nose_strings[].
 */
enum cnc_endmill_nose_type {
	CNC_ENDMILL_FLAT,		/* Flat nose */
	CNC_ENDMILL_ROUNDED,		/* Rounded-edge */
	CNC_ENDMILL_TAPERED,		/* Taper degree end mill */
	CNC_ENDMILL_BALL,		/* Ball-nose */
	CNC_ENDMILL_CHAMFER,		/* For bevel/angle cuts */
	CNC_ENDMILL_DOVETAIL,		/* Dovetail cutter */
	CNC_ENDMILL_TSLOT,		/* T-slot cutter */
	CNC_ENDMILL_CORNER_ROUNDING,	/* Corner rounding */
	CNC_ENDMILL_CONVEX_RADIUS,	/* Produce hollow inward-curving shape */
	CNC_ENDMILL_CONCAVE_RADIUS,	/* Produce rounded outward-curving shape */
};

/* For CNC_GETTOOLINFO/CNC_SETTOOLINFO */
struct cnc_tool {
	int idx;		/* Tool index */
	int atc;		/* Associated atc(4) device (-1 = none) */
	enum cnc_tool_type type; /* Tool class */
	cnc_dist_t lenOffs;	/* Tool length offset */
	cnc_dist_t wearFn;	/* Tool wear factor */
	cnc_dist_t wearOffs;	/* Effective wear offset */
	union {
		struct {
			cnc_dist_t dia;	/* Nominal diameter */
			cnc_dist_t len;	/* Flute length */
			int ptAngle;	/* Point / countersink angle (degs) */
		} drill;
		struct {
			cnc_dist_t id;	/* Inside diameter */
			cnc_dist_t od;	/* Outside diameter */
			cnc_dist_t len;	/* Usable length */
		} holesaw;
		struct {
			enum cnc_endmill_nose_type nose;	/* Shape */
			cnc_dist_t dia;	/* Nominal dia (tip dia for TAPERED) */
			cnc_dist_t len;	/* Length of cut (face wd. for TSLOT) */
			cnc_dist_t r;	/* Radius (for ROUNDED/CORNER_ROUNDING/RADIUS) */
			int angle;	/* Angle (for CHAMFER/DOVETAIL) */
		} endmill;
	} data;
};

/* For CNC_GETSTATS */
struct cnc_stats {
	cnc_vel_t peak_velocity;	/* Peak velocity reached (steps/s) */
	int estops;			/* # of emergency stop events */
};

enum cnc_input_event_type {
	CNC_EVENT_KEYDOWN,		/* Keyboard event */
	CNC_EVENT_KEYUP,
	CNC_EVENT_BUTTONDOWN,		/* Button event */
	CNC_EVENT_BUTTONUP,
	CNC_EVENT_WHEELDOWN,		/* Wheel (e.g., MPG) motion */
	CNC_EVENT_WHEELUP,
	CNC_EVENT_SELECTOR,		/* Selector switch event */
	CNC_EVENT_LAST
};

/* General input-device event. */
struct cnc_input_event {
	enum cnc_input_event_type type;	/* Event type */
	int which;			/* Device index */
	int data;			/* Per-device data */
};

/* For /dev/mpg* events */
struct cnc_mpg_event {
	int axis;			/* Selected axis */
	int delta;			/* Pulses received */
};

#define CNC_GETPOS		_IOR('C', 0, struct cnc_vector)
#define CNC_SETPOS		_IOWR('C', 1, struct cnc_vector)
#define CNC_GETDEVICEINFO	_IOR('C', 2, struct cnc_device_info)
#define CNC_GETKINLIMITS	_IOR('C', 3, struct cnc_kinlimits)
#define CNC_SETKINLIMITS	_IOWR('C', 4, struct cnc_kinlimits)
#define CNC_GETTIMINGS		_IOR('C', 5, struct cnc_timings)
#define CNC_SETTIMINGS		_IOWR('C', 6, struct cnc_timings)
#define CNC_CALTIMINGS		_IOWR('C', 7, struct cnc_timings)
#define CNC_SETCAPTUREMODE	_IOWR('C', 8, int)
#define CNC_GETSTATS		_IOR('C', 9, struct cnc_stats)
#define CNC_CLRSTATS		_IO('C', 10)
#define CNC_GETNTOOLS		_IOR('C', 11, int)
#define CNC_GETTOOLINFO		_IOR('C', 12, struct cnc_tool)
#define CNC_SETTOOLINFO		_IOWR('C', 13, struct cnc_tool)

#if !defined(_KERNEL)

#include <sys/cdefs.h>

__BEGIN_DECLS
int	cncmove(const struct cnc_velocity *, const cnc_vec_t *);
int	cncspotweld(int, int);
int	cncspotweldtrig(int);
int	cncspotweldselect(int);
int	spinctl(int, enum cnc_spindle_op, const struct cnc_spindle_args *);
int	atcctl(int, enum cnc_atc_op, const struct cnc_atc_args *);
int	laserctl(int, enum cnc_laser_op, const struct cnc_laser_args *);
int	pickplacectl(int, enum cnc_pickplace_op, const struct cnc_pickplace_args *);
int	coolantctl(int, enum cnc_coolant_op, const struct cnc_coolant_args *);
int	estop(void);
int	cncpos(cnc_vec_t *);
__END_DECLS

#endif /* !_KERNEL */

#endif	/* !_SYS_CNC_H_ */
