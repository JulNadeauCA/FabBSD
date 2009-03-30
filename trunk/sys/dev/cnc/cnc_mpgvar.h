/*	Public domain	*/

enum mpg_pin {
	MPG_PIN_A,
	MPG_PIN_B,
	MPG_PIN_SELX,
	MPG_PIN_SELY,
	MPG_PIN_SELZ,
	MPG_PIN_SELA,
	MPG_PIN_SELB,
	MPG_PIN_SELC,
	MPG_PIN_LAST
};

struct mpg_axis {
	int A, B;			/* Current A & B signals */
	int Aprev, Bprev;		/* Previous A & B signals */
};

struct mpg_softc {
	struct cnc_device sc_cdev;
	void *sc_gpio;
	struct gpio_pinmap sc_map;
	int __map[MPG_PIN_LAST];
	int sc_naxes;				/* Number of axes */
	struct mpg_axis sc_axes[CNC_MAX_AXES]; /* Per-axis signal status */
	int sc_sel_axis;			/* Currently selected axis */
	int sc_mult;				/* Current multiplier */
	int sc_pulses[CNC_MAX_AXES];		/* Pulse counts */
	int sc_ppi;				/* Pulses per increment */
};

/*
 * Evaluate whether a quadrature signal transition [AB]prev -> [AB]
 * indicates clockwise rotation.
 */
#define MPG_TRANSITION_CW(axis)						\
    ((!(axis)->A && !(axis)->Aprev &&  (axis)->B && !(axis)->Bprev) ||	\
     ( (axis)->A && !(axis)->Aprev &&  (axis)->B &&  (axis)->Bprev) ||	\
     ( (axis)->A &&  (axis)->Aprev && !(axis)->B &&  (axis)->Bprev))

/*
 * Evaluate whether a quadrature signal transition [AB]prev -> [AB]
 * indicates counterclockwise rotation.
 */
#define MPG_TRANSITION_CCW(axis)					\
    (( (axis)->A && !(axis)->Aprev && !(axis)->B && !(axis)->Bprev) ||	\
     ( (axis)->A &&  (axis)->Aprev &&  (axis)->B && !(axis)->Bprev) ||	\
     (!(axis)->A &&  (axis)->Aprev &&  (axis)->B &&  (axis)->Bprev))

int  mpg_get_axis(struct mpg_softc *);
void mpg_jog_init(struct mpg_softc *);
void mpg_jog(struct mpg_softc *, cnc_vec_t *, int);

