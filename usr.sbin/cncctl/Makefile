# $FabBSD$

PROG=	cncctl
SRCS=	cncctl.c
MAN=	cncctl.8
DPADD= ${LIBCNC} ${LIBM}
LDADD= -lcnc -lm

.include <bsd.prog.mk>

CFLAGS+=-Wall -Werror
