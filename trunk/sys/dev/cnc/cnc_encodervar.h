/*	Public domain	*/

#define ENCODER_NPINS		2
#define ENCODER_PIN_A		0
#define ENCODER_PIN_B		1

struct encoder_softc {
	struct cnc_device sc_cdev;
	void *sc_gpio;
	struct gpio_pinmap sc_map;
	int __map[ENCODER_NPINS];
	int sc_state_a;				/* last A state */
	int sc_state_b;				/* last B state */
};

void encoder_update(struct encoder_softc *);

