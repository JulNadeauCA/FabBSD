#	$FabBSD$
#	from: @(#)Makefile	5.13 (Berkeley) 5/7/91

FILES=	access.bus ide40 idepow parport-host parport-printer \
	rs232-de25 rs232-de9 rs232-rj45

all clean cleandir depend lint obj tags: _SUBDIRUSE

realinstall:
	cd ${.CURDIR}; install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
	    ${DESTDIR}${BINDIR}/pinouts

.include <bsd.prog.mk>
