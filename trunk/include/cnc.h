/*	Public domain	*/

#ifndef _CNC_H_
#define _CNC_H_

#include <sys/types.h>
#include <sys/cnc.h>
#include <sys/cdefs.h>

/* User libcnc configuration data */
struct cnc_config {
	char	**keys;
	char	**vals;
	u_int	 nvals;
};

/* Unit of measurement */
typedef struct cnc_unit {
	char *key;			/* Key */
	char *abbr;			/* Symbol */
	char *name;			/* Long name */
	double divider;			/* Base unit divider (for linear conv) */
	double (*func)(double, int);	/* For nonlinear conversions */
} cnc_unit_t;

/* Symbol for standard drill size */
struct cnc_drill_size {
	const char *key;
	cnc_real_t v;
};

__BEGIN_DECLS
extern struct cnc_velocity cnc_vel_default;
extern const char *cnc_tool_strings[];		/* For enum cnc_tool_type */
extern const char *cnc_endmill_nose_strings[];	/* For enum cnc_endmill_nose_type */
extern const cnc_real_t cnc_drill_letter[];	/* Letter drill sizes */
extern const cnc_real_t cnc_drill_wire[];	/* Wire drill sizes */

/*
 * Library initialization and error handling
 */
int         cnc_init(void);
void        cnc_destroy(void);
const char *cnc_get_error(void);
void        cnc_set_error(const char *, ...);

/*
 * Tool specification
 */
int         cnc_tool_parse(const char *, struct cnc_tool *);
void        cnc_tool_print(const struct cnc_tool *);


/*
 * User configuration data
 */
struct cnc_config *cnc_config_open(const char *);
void               cnc_config_close(struct cnc_config *);
void    cnc_config_int(struct cnc_config *, const char *, int *, int);
#define cnc_config_bool cnc_config_int
void    cnc_config_uint(struct cnc_config *, const char *, u_int *, u_int);
void    cnc_config_long(struct cnc_config *, const char *, long *, long);
void    cnc_config_ulong(struct cnc_config *, const char *, u_long *, u_long);
#define cnc_config_vel cnc_config_ulong
void    cnc_config_flt(struct cnc_config *, const char *, float *, float);
void    cnc_config_dbl(struct cnc_config *, const char *, double *, double);
void    cnc_config_string(struct cnc_config *, const char *, char **, const char *);
void    cnc_config_strlcpy(struct cnc_config *, const char *, char *, size_t, const char *);

/*
 * Linear algebra routines
 */
int         cnc_vec_parse(cnc_vec_t *, const char *);
void        cnc_vec_zero(cnc_vec_t *);
void        cnc_vec_sub(cnc_vec_t *, const cnc_vec_t *, const cnc_vec_t *);
void        cnc_vec_add(cnc_vec_t *, const cnc_vec_t *, const cnc_vec_t *);
cnc_real_t  cnc_vec_dotprod(const cnc_vec_t *, const cnc_vec_t *);
cnc_real_t  cnc_vec_length(const cnc_vec_t *);
cnc_real_t  cnc_vec_distance(const cnc_vec_t *, const cnc_vec_t *);
int         cnc_vec_print(const cnc_vec_t *, char *, size_t)
                __attribute__((__bounded__(__string__,2,3)));
int         cnc_vel_parse(cnc_vel_t *, const char *);
int         cnc_dist_parse(cnc_dist_t *, const char *);

/*
 * Units of measurement
 */
const cnc_unit_t *cnc_find_unit(const char *, const cnc_unit_t *);
const cnc_unit_t *cnc_find_unit_all(const char *);
const cnc_unit_t *cnc_best_unit(const cnc_unit_t[], double);
int	   	  cnc_unit_format(double, const cnc_unit_t[], char *, size_t);

extern const cnc_unit_t *cnc_unit_groups[];
extern const char       *cnc_unit_group_names[];
extern const int         cnc_unit_groups_count;

extern const cnc_unit_t cncIdentityUnit[];
extern const cnc_unit_t cncLengthUnits[];
extern const cnc_unit_t cncAngleUnits[];
extern const cnc_unit_t cncAreaUnits[];
extern const cnc_unit_t cncVolumeUnits[];
extern const cnc_unit_t cncSpeedUnits[];
extern const cnc_unit_t cncMassUnits[];
extern const cnc_unit_t cncTimeUnits[];
extern const cnc_unit_t cncCurrentUnits[];
extern const cnc_unit_t cncTemperatureUnits[];
extern const cnc_unit_t cncLightUnits[];
extern const cnc_unit_t cncPowerUnits[];
extern const cnc_unit_t cncEMFUnits[];
extern const cnc_unit_t cncResistanceUnits[];
extern const cnc_unit_t cncCapacitanceUnits[];
extern const cnc_unit_t cncInductanceUnits[];
extern const cnc_unit_t cncFrequencyUnits[];
extern const cnc_unit_t cncPressureUnits[];
extern const cnc_unit_t cncVacuumUnits[];

static __inline__ double
cnc_unit2base(double n, const cnc_unit_t *unit)
{
	return (unit->func != NULL ? unit->func(n, 1) : n*unit->divider);
}
static __inline__ double
cnc_base2unit(double n, const cnc_unit_t *unit)
{
	return (unit->func != NULL ? unit->func(n, 0) : n/unit->divider);
}
static __inline__ double
cnc_unit2unit(double n, const cnc_unit_t *ufrom, const cnc_unit_t *uto)
{
	return (cnc_base2unit(cnc_unit2base(n, ufrom), uto));
}
static __inline__ const char *
cnc_unit_abbr(const cnc_unit_t*unit)
{
	return (unit->abbr[0] != '\0' ? unit->abbr : unit->key);
}

#define	cnc_unit2basef(n, u) ((float)cnc_unit2base((float)(n), (u)))
#define	cnc_base2unitf(n, u) ((float)cnc_base2unit((float)(n), (u)))
#define	cnc_unit2unitf(n, u1, u2) ((float)cnc_unit2unit((float)(n), (u1), (u2)))
__END_DECLS

#endif /* _CNC_H_ */
