/*	Public domain	*/

#define ESTOP_NPINS	1
#define ESTOP_PIN	0

struct estop_softc {
	struct cnc_device sc_cdev;

	int sc_flags;
	void *sc_gpio;
	struct gpio_pinmap sc_map;
	int __map[ESTOP_NPINS];
};

int estop_get_state(struct estop_softc *);
