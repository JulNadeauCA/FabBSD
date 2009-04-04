/*	Public domain	*/

#ifndef _CNC_H_
#define _CNC_H_

#include <sys/types.h>
#include <sys/cnc.h>
#include <sys/cdefs.h>

__BEGIN_DECLS
const char *cnc_get_error(void);
void        cnc_set_error(const char *, ...);

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
__END_DECLS
#endif /* _CNC_H_ */
