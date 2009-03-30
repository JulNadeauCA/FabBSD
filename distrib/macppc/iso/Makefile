#	$FabBSD$
#	$OpenBSD: Makefile,v 1.4 2007/08/10 12:29:55 deraadt Exp $

TOP=		${.CURDIR}/..

#.include "${TOP}/Makefile.inc"

CDROM=		install${OSrev}.iso
RELXDIR=	/home/relx-${MACHINE}
RELDIR?=	/home/rel-${MACHINE}

BASE=		${RELDIR}/base${OSrev}.tgz ${RELDIR}/comp${OSrev}.tgz \
		${RELDIR}/etc${OSrev}.tgz ${RELDIR}/man${OSrev}.tgz \
		${RELDIR}/misc${OSrev}.tgz \
		${RELDIR}/bsd ${RELDIR}/bsd.rd \
		${RELDIR}/INSTALL.${MACHINE}

all: ${CDROM}

${CDROM}: ${BASE}
	-rm -rf ${.OBJDIR}/cd-dir/
	-@mkdir -p ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}/

	cp -p ${BASE} ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}

	cp -p ${.CURDIR}/../../../sys/arch/${MACHINE}/stand/tbxidata/bsd.tbxi \
	    ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}
	cp -p ${RELDIR}/ofwboot ${.OBJDIR}/cd-dir/

	mkhybrid -v -v -r -part -hfs \
	    -hfs-bless ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE} \
	    -map ${.CURDIR}/../../../gnu/usr.sbin/mkhybrid/src/more.mapping \
	    -A "FabBSD ${OSREV} ${MACHINE} Install CD" \
	    -P "Copyright (c) `date +%Y` Hypertriton, Inc." \
	    -p "Julien Nadeau <vedge@hypertriton.com>" \
	    -V "FabBSD/${MACHINE}  ${OSREV} Install CD" \
	    -o ${.OBJDIR}/${CDROM} ${.OBJDIR}/cd-dir

install:
	cp ${CDROM} ${RELDIR}/

clean cleandir:
	/bin/rm -f ${CDROM}
	rm -rf cd-dir

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
