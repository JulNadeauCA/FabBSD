/*	Public domain	*/

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <sys/cnc.h>

#include "cnc_devicevar.h"
#include "cncvar.h"

void
cnc_vec_init(cnc_vec_t v)
{
	int i;

	for (i = 0; i < CNC_NAXES; i++)
		v.v[i] = 0;
}

/* Return (v1-v2) in vDst. */
void
cnc_vec_sub(cnc_vec_t *vDst, const cnc_vec_t *v1, const cnc_vec_t *v2)
{
	int i;

	for (i = 0; i < CNC_NAXES; i++)
		vDst->v[i] = v1->v[i] - v2->v[i];
}

/* Return (v1+v2) in vDst. */
void
cnc_vec_add(cnc_vec_t *vDst, const cnc_vec_t *v1, const cnc_vec_t *v2)
{
	int i;

	for (i = 0; i < CNC_NAXES; i++)
		vDst->v[i] = v1->v[i] + v2->v[i];
}

/* Return real dot product of v1 and v2. */
cnc_real_t
cnc_vec_dotprod(const cnc_vec_t *v1, const cnc_vec_t *v2)
{
	cnc_real_t dot = 0.0;
	int i;

	for (i = 0; i < CNC_NAXES; i++) {
		dot += ((cnc_real_t)v1->v[i])*((cnc_real_t)v2->v[i]);
	}
	return (dot);
}

/* Return real length of vector v. */
cnc_real_t
cnc_vec_length(const cnc_vec_t *v)
{
	cnc_real_t len_2 = 0.0;
	int i;

	for (i = 0; i < CNC_NAXES; i++) {
		len_2 += ((cnc_real_t)v->v[i])*((cnc_real_t)v->v[i]);
	}
	return sqrt(len_2);
}

/* Return real distance between vectors v1 and v2. */
cnc_real_t
cnc_vec_distance(const cnc_vec_t *v1, const cnc_vec_t *v2)
{
	cnc_vec_t vd;

	cnc_vec_sub(&vd, v2, v1);
	return cnc_vec_length(&vd);
}

/* Return 1 if the two arguments are the same vector. */
int
cnc_vec_same(const cnc_vec_t *v1, const cnc_vec_t *v2)
{
	int i;

	for (i = 0; i < CNC_NAXES; i++) {
		if (v1->v[i] != v2->v[i])
			return (0);
	}
	return (1);
}

char *
cnc_fmt_real(cnc_real_t v)
{
	static char s[64];
	double fpart;
	double ipart;

	fpart = modf(v, &ipart);
	snprintf(s, sizeof(s), "%ld.%ld", (long)ipart, (long)(fpart*10000.0));
	return (s);
}
