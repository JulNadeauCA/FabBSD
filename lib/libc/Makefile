#	$FabBSD$
#	$OpenBSD: Makefile,v 1.26 2006/03/14 09:19:37 deraadt Exp $
#
# All library objects contain sccsid strings by default; they may be
# excluded as a space-saving measure.  To produce a library that does
# not contain these strings, delete -DLIBC_SCCS and -DSYSLIBC_SCCS
# from CFLAGS below.  To remove these strings from just the system call
# stubs, remove just -DSYSLIBC_SCCS from CFLAGS.
#
# The NLS (message catalog) functions are always in libc.  To choose that
# strerror(), perror(), strsignal(), psignal(), etc. actually call the NLS
# functions, put -DNLS on the CFLAGS line below.
#
# The YP functions are always in libc. To choose that getpwent() and friends
# actually call the YP functions, put -DYP on the CFLAGS line below.

.include <bsd.own.mk>

LIB=c
CLEANFILES+=tags
#CFLAGS+=-Werror

LIBCSRCDIR=${.CURDIR}
.include "${LIBCSRCDIR}/Makefile.inc"

NLS=	C.msg Pig.msg de.msg es.msg fi.msg fr.msg nl.msg no.msg ru.msg sv.msg it.msg

copy-to-libkern:	copy-to-libkern-machind copy-to-libkern-machdep

.if make(copy-to-libkern)
copy-to-libkern-machind: ${KSRCS}
	cp -p ${.ALLSRC} ${LIBKERN}
.if defined(KINCLUDES) && !empty(KINCLUDES)
	(cd ${.CURDIR} ; cp -p ${KINCLUDES} ${LIBKERN})
.endif

copy-to-libkern-machdep: ${KMSRCS}
.if defined(KMSRCS) && !empty(KMSRCS)
	cp -p ${.ALLSRC} ${LIBKERN}/arch/${MACHINE_ARCH}
.endif
.if defined(KMINCLUDES) && !empty(KMINCLUDES)
	(cd ${.CURDIR} ; cp -p ${KMINCLUDES} ${LIBKERN}/arch/${MACHINE_ARCH})
.endif

rm-from-libkern:
	for i in ${KSRCS}; do rm -f ${LIBKERN}/$$i; done
.if defined(KMSRCS) && !empty(KMSRCS)
	for i in ${KMSRCS}; do rm -f ${LIBKERN}/arch/${MACHINE_ARCH}/$$i; done
.endif
.endif

all: tags
tags: ${SRCS}
	ctags -w ${.ALLSRC:M*.c}
	egrep "^SYSENTRY(.*)|^ENTRY(.*)|^FUNC(.*)|^SYSCALL(.*)" \
	    /dev/null ${.ALLSRC:M*.S} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3	\1	/^\2(\3\4$$/;" \
	    >> tags; sort -o tags tags

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 tags \
		${DESTDIR}/var/db/lib${LIB}.tags

.include <bsd.lib.mk>
