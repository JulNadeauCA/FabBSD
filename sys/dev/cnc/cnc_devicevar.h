/*	Public domain	*/

enum cnc_device_type {
	CNC_DEVICE_ESTOP,		/* Emergency stop device */
	CNC_DEVICE_SERVO,		/* Servo motor controller */
	CNC_DEVICE_SPINDLE,		/* Machine spindle controller */
	CNC_DEVICE_ATC,			/* Automatic tool changer */
	CNC_DEVICE_LASER,		/* Laser power supply controller */
	CNC_DEVICE_PICKPLACE,		/* Pick and place controller */
	CNC_DEVICE_ENCODER,		/* Position-sensing encoder (general-purpose) */
	CNC_DEVICE_MPG,			/* Manual pulse generator */
	CNC_DEVICE_LCD			/* Serial interface to text-based LCD */
};

struct cnc_device {
	struct device cd_dev;
	enum cnc_device_type cd_type;
	int cd_flags;
	TAILQ_ENTRY(cnc_device) devices;
};

TAILQ_HEAD(cnc_deviceq,cnc_device);
extern struct cnc_deviceq cnc_devices;

int cnc_device_attach(void *, enum cnc_device_type);
int cnc_device_detach(void *);
int cnc_device_activate(void *);

/* Attach routine: Map a standard input pin */
#define CNC_MAP_INPUT(sc, n, name) \
	do { \
		int caps; \
		caps = gpio_pin_caps((sc)->sc_gpio, &(sc)->sc_map, (n)); \
		if (!(caps & GPIO_PIN_INPUT)) { \
			printf(": pin#%d (%s) cannot be used for input\n", \
			    (n), (name)); \
			goto fail; \
		} \
		printf(" %s[%d]", name, (sc)->sc_map.pm_map[n]); \
		gpio_pin_ctl((sc)->sc_gpio, &(sc)->sc_map, (n), GPIO_PIN_INPUT); \
	} while (/*CONSTCOND*/0)

/* Attach routine: Map a standard output pin */
#define CNC_MAP_OUTPUT(sc, n, name) \
	do { \
		int caps, ctl; \
		caps = gpio_pin_caps((sc)->sc_gpio, &(sc)->sc_map, (n)); \
		if (!(caps & GPIO_PIN_OUTPUT)) { \
			printf(": pin#%d (%s) cannot drive output\n", \
			    (n), (name)); \
			goto fail; \
		} \
		printf(" >%s[%d]", name, (sc)->sc_map.pm_map[n]); \
		ctl = GPIO_PIN_OUTPUT; \
		if (caps & GPIO_PIN_PUSHPULL) { \
			ctl |= GPIO_PIN_PUSHPULL; \
		} \
		gpio_pin_ctl((sc)->sc_gpio, &(sc)->sc_map, (n), ctl); \
	} while (/*CONSTCOND*/0)

