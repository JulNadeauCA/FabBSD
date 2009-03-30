/*	$OpenBSD$	*/
/*	Public domain	*/

#define SERVO_NPINS		2
#define SERVO_PIN_STEP		0
#define SERVO_PIN_DIR		1

struct servo_softc {
	struct cnc_device sc_cdev;
	void *sc_gpio;
	struct gpio_pinmap sc_map;
	int __map[SERVO_NPINS];
	int sc_step;				/* last step */
	int sc_dir;				/* last direction */
};

void servo_set_dir(struct servo_softc *, int);
void servo_step(struct servo_softc *, int);
