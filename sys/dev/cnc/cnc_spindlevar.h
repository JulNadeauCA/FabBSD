/*	Public domain	*/

#define SPINDLE_NPINS		4
#define SPINDLE_PIN_DIR		0	/* direction signal (0=CW, 1=CCW) */
#define SPINDLE_PIN_TACHO	1	/* tachometer input (1 pulse/rev) */
#define SPINDLE_PIN_INCREMENT	2	/* speed controller increment */
#define SPINDLE_PIN_DECREMENT	3	/* speed controller decrement */

struct spindle_softc {
	struct cnc_device sc_cdev;
	void *sc_gpio;
	struct gpio_pinmap sc_map;
	int __map[SERVO_NPINS];
	int sc_dir;			/* current direction (1=CW, -1=CCW) */
	int sc_speed;			/* desired speed (rpm) */
	u_int sc_speed_tacho;		/* last measured speed (rpm) */
	u_int sc_speed_max;		/* maximum allowed speed (rpm) */
};

struct spindle_softc *spindle_find(int);

