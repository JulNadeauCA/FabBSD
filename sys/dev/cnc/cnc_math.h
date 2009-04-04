/*	Public domain	*/

#include <lib/libkern/libkern.h>
#include <lib/libkern/math.h>

#define CNC_MSECS(s)	((u_long)(s*1e3))

char      *cnc_fmt_real(cnc_real_t);
void       cnc_vec_init(cnc_vec_t);
void       cnc_vec_sub(cnc_vec_t *, const cnc_vec_t *, const cnc_vec_t *);
void       cnc_vec_add(cnc_vec_t *, const cnc_vec_t *, const cnc_vec_t *);
cnc_real_t cnc_vec_dotprod(const cnc_vec_t *, const cnc_vec_t *);
cnc_real_t cnc_vec_length(const cnc_vec_t *);
cnc_real_t cnc_vec_distance(const cnc_vec_t *, const cnc_vec_t *);
int        cnc_vec_same(const cnc_vec_t *, const cnc_vec_t *);
