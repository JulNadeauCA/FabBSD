#	$OpenBSD: Makefile,v 1.34 2008/05/13 21:15:26 miod Exp $

SUBDIR=	special notes

.if	make(obj)
SUBDIR+=crunch
SUBDIR+=alpha amd64 armish i386 macppc sparc sparc64
.elif exists(${MACHINE})
SUBDIR+=${MACHINE}
.endif

.if	make(clean) || make(cleandir)
SUBDIR+=crunch
.endif

#all: crunch-tools _SUBDIRUSE

crunch-tools:
	cd $(.CURDIR)/crunch; $(MAKE) obj; \
	 $(MAKE) depend && ${MAKE} && ${SUDO} ${MAKE} DESTDIR= install

.include <bsd.subdir.mk>
