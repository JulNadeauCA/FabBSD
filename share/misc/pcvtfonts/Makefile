#	$FabBSD$
#	$OpenBSD: Makefile,v 1.6 2002/05/13 15:27:58 mickey Exp $

FONTS	= 	vt220l.814 vt220h.814 vt220l.808 vt220h.808 \
		vt220l.816 vt220h.816 vt220l.810 vt220h.810 \
		koi8-r-8x08 koi8-r-8x10 koi8-r-8x14 koi8-r-8x16 \
		koi8-u-8x08 koi8-u-8x10 koi8-u-8x14 koi8-u-8x16 \
		iso8859-13.808 iso8859-13.810 iso8859-13.814 iso8859-13.816 \
		iso8859-1-euro.816

LIBMODE	=	444
LIBOWN	=	bin
LIBGRP	=	bin
FONTDIR =	${BINDIR}/misc/pcvtfonts

all: $(FONTS)

install: ${FONTS}
	@if [ ! -d ${DESTDIR}${FONTDIR} ]; then mkdir ${DESTDIR}${FONTDIR};fi
	@for i in ${FONTS}; do \
		echo "installing font $$i into ${DESTDIR}${FONTDIR}"; \
		install -c -m ${LIBMODE} -o ${LIBOWN} -g ${LIBGRP} \
		${.CURDIR}/$$i ${DESTDIR}${FONTDIR}; \
	done

.include "../../Makefile.inc"
.include <bsd.prog.mk>
