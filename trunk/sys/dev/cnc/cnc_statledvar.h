/*	Public domain	*/

struct cncstatled_softc {
	struct cnc_device sc_cdev;
	void *sc_gpio;
	struct gpio_pinmap sc_map;
	int __map[1];
};

void cncstatled_set(int);
