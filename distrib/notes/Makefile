#	$FabBSD$
#	$OpenBSD: Makefile,v 1.31 2008/05/15 21:01:14 miod Exp $

NOPROG=
NOMAN=

M?=	$(MACHINE)

.if exists(${M}/contents)

TARG=	INSTALL.$M
SRC=	$(.CURDIR)/INSTALL
DEP=	$(SRC) $(.CURDIR)/mirrors $(.CURDIR)/$M/whatis $(.CURDIR)/$M/contents \
	$(.CURDIR)/$M/hardware $(.CURDIR)/$M/xfer $(.CURDIR)/$M/prep \
	$(.CURDIR)/$M/install $(.CURDIR)/$M/upgrade \
	$(.CURDIR)/packages $(.CURDIR)/m4.common

all:	$(TARG)

allarchs:
	@cd $(.CURDIR); \
	for arch in alpha amd64 armish i386 macppc sparc sparc64; do \
		${MAKE} M=$$arch; \
	done

$(TARG): $(DEP)
	m4 -DOSREV=$(OSREV) -DOSrev=$(OSrev) -DINCLUDE=$(.CURDIR)/$M \
	-DMACHINE=$M -Uunix $(SRC) > $@

beforeinstall:
	cp ${TARG} ${DESTDIR}/snapshot/

.else
all:
.endif

clean:
	-/bin/rm -f INSTALL.*

.include <bsd.prog.mk>
