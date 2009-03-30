#	$FabBSD$
#	$OpenBSD: Makefile,v 1.8 2004/02/10 02:02:22 espie Exp $

.include <bsd.own.mk>

NOMAN=

PROG=cpp
SRCS=
INSTALL_STRIP=

cpp: cpp.sh
	sed -e 's/@dollaropt@//' ${.CURDIR}/cpp.sh >$@

.include <bsd.prog.mk>
