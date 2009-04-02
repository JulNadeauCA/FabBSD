/*	$FabBSD$	*/
/*	Public domain	*/

#define LPTGPIO_NPINS		17

struct lptgpio_softc {
	struct device		sc_dev;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	struct gpio_chipset_tag	sc_gpio_gc;
	struct gpio_pin		sc_gpio_pins[LPTGPIO_NPINS];
};

void	lptgpio_attach_common(struct lptgpio_softc *);
int	lptgpio_pin_read(void *, int);
void	lptgpio_pin_write(void *, int, int);
void	lptgpio_pin_ctl(void *, int, int);
