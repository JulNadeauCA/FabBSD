#	$FabBSD$

LIB=	cnc

SRCS=	cnc.c cnc_distance.c cnc_vector.c cnc_velocity.c

.if	exists(cnc_${MACHINE}.c)
SRCS+=	cnc_${MACHINE}.c
.endif

MAN=	cnc.3 cnc_dist_parse.3 cnc_get_error.3 cnc_vel_parse.3 \
	cnc_set_error.3 cnc_vec_add.3 cnc_vec_compare.3 cnc_vec_distance.3 \
	cnc_vec_dotprod.3 cnc_vec_length.3 cnc_vec_parse.3 cnc_vec_print.3 \
	cnc_vec_sub.3 cnc_vec_zero.3

.include <bsd.lib.mk>
