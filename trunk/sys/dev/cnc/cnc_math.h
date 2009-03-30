/*	Public domain	*/

typedef float cnc_real_t;

#define	CNC_PI		3.14159265358979323846	/* pi */
#define	CNC_PI_2	1.57079632679489661923	/* pi/2 */
#define	CNC_PI_P2	(CNC_PI*CNC_PI)		/* pi^2 */
#define CNC_MSECS(s)	((u_long)(s*1e3))

double cnc_sin(double);
double cnc_sqrt(double);
double cnc_cbrt(double);
double cnc_log10(double);
double cnc_modf(double, double *);

char  *cnc_fmt_real(cnc_real_t);

void       cnc_vec_init(cnc_vec_t);
void       cnc_vec_sub(cnc_vec_t *, const cnc_vec_t *, const cnc_vec_t *);
void       cnc_vec_add(cnc_vec_t *, const cnc_vec_t *, const cnc_vec_t *);
cnc_real_t cnc_vec_dotprod(cnc_vec_t *, const cnc_vec_t *, const cnc_vec_t *);
cnc_real_t cnc_vec_length(const cnc_vec_t *);
cnc_real_t cnc_vec_distance(const cnc_vec_t *, const cnc_vec_t *);
int        cnc_vec_same(const cnc_vec_t *, const cnc_vec_t *);
