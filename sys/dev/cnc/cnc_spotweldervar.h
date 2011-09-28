/*	$OpenBSD$	*/
/*	Public domain	*/

#define SPOTWELDER_NPINS	4

enum spotwelder_pins {
	SPOTWELDER_PIN_RELAY	= 0,
	SPOTWELDER_PIN_PHASE	= 1,
	SPOTWELDER_PIN_TRIGGER	= 2,
	SPOTWELDER_PIN_SELECT 	= 3
};

#define SPOTWELDER_MAXCYCLES		10000

struct spotwelder_softc {
	struct cnc_device sc_cdev;
	void *sc_gpio;
	struct gpio_pinmap sc_map;
	int __map[SPOTWELDER_NPINS];
};

int spotwelder_trig(struct spotwelder_softc *);
int spotwelder_select(struct spotwelder_softc *);
int spotwelder_weld(struct spotwelder_softc *, int);
