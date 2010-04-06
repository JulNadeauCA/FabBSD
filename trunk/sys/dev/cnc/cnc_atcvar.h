/*	Public domain	*/

#define ATC_NPINS		4

enum atc_pin {
	ATC_PIN_PREP_NEXT =	0,
	ATC_PIN_PREP_PREV =	1,
	ATC_PIN_CHANGE =	2,
	ATC_PIN_READY =		3,
};

struct atc_softc {
	struct cnc_device sc_cdev;
	void *sc_gpio;
	struct gpio_pinmap sc_map;
	int __map[SERVO_NPINS];
	int sc_nslots;			/* total # of slots */
	int sc_curslot;			/* slot in use */
};

int               atc_toolchange(void *, int);
struct atc_softc *atc_find(int);

