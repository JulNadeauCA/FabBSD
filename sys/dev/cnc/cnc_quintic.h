/*	Public domain	*/

struct cnc_quintic_profile {
	cnc_real_t F;			/* User-given feed rate */
	cnc_real_t v0;			/* User-given min velocity */
	cnc_real_t Aref;		/* Effective acceleration */
	cnc_real_t Ts;			/* Domain of third-order sine */
	cnc_real_t Ta;			/* Total acceleration time */
	cnc_real_t To;			/* Total time before deceleration */
	cnc_real_t v1;			/* Velocity at Ts */
	cnc_real_t v2;			/* Velocity at Ta-Ts */
};

int        cnc_quintic_init(struct cnc_quintic_profile *, cnc_real_t,
                            cnc_real_t, cnc_real_t, cnc_real_t, cnc_real_t);
cnc_real_t cnc_quintic_step(const struct cnc_quintic_profile *, cnc_real_t);
