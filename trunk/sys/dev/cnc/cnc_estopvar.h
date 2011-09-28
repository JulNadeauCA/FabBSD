/*	Public domain	*/

#define ESTOP_NPINS	1
#define ESTOP_PIN	0

struct estop_softc {
	struct cnc_device sc_cdev;

	int sc_flags;
#define ESTOP_DEBOUNCE		0x0001

	void *sc_gpio;
	struct gpio_pinmap sc_map;
	int __map[ESTOP_NPINS];
	int sc_open;				/* Device is open */
	int sc_queued;				/* Estop event queued */
};

int estop_debounce(struct estop_softc *);
int estop_get_state(struct estop_softc *);
