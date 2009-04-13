#	$FabBSD$

PROG=	ngc
SRCS=	ngc.c util.c block.c vector.c

DPADD= ${LIBCNC} ${LIBM}
LDADD= -lcnc -lm

.include <bsd.prog.mk>

afterinstall:
	@if [ ! -e "${DESTDIR}/var/db" ]; then \
		echo "${INSTALL} -d ${DESTDIR}/var/db"; \
		${INSTALL} -d ${DESTDIR}/var/db; \
	fi
	@if [ -e "${DESTDIR}/var/db/ngc-params" ]; then \
		echo "* /var/db/ngc-params exists, not overwriting"; \
	else \
		echo "${INSTALL} -m ${NONBINMODE} -o ${BINOWN} -g ${BINGRP} \
		 ngc-params ${DESTDIR}/var/db/ngc-params"; \
		(cd ${.CURDIR} && \
		 ${INSTALL} -m ${NONBINMODE} -o ${BINOWN} -g ${BINGRP} \
		 ngc-params ${DESTDIR}/var/db/ngc-params); \
	fi
