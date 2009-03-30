#	$FabBSD$
#	$OpenBSD: Makefile,v 1.4 2008/03/05 21:28:36 deraadt Exp $

TOP=	${.CURDIR}/..

.include "${TOP}/Makefile.inc"

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
	-rm -rf ${.OBJDIR}/cd-dir
	mkdir -p ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}

	cp -p ${BASE} ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}

	# make sure right kernel is in /
	ln ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}/bsd.rd ${.OBJDIR}/cd-dir/bsd.rd
	ln ${.OBJDIR}/cd-dir/bsd.rd ${.OBJDIR}/cd-dir/bsd

	cp -p ${RELDIR}/boot ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}
	cp -p ${RELDIR}/bootxx ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}

	(mkhybrid -a -R -v -v -T -L -d -D -N -o ${.OBJDIR}/${CDROM} \
	    -A "FabBSD ${OSREV} ${MACHINE} Install CD" \
	    -P "Copyright (c) `date +%Y` Hypertriton, Inc." \
	    -p "Julien Nadeau <vedge@hypertriton.com>" \
	    -V "FabBSD/${MACHINE}   ${OSREV} Install CD" \
	    ${.OBJDIR}/cd-dir 2>&1) | tee log
	vnconfig -v -c svnd0 ${.OBJDIR}/${CDROM}
	mount -t cd9660 /dev/svnd0a /mnt
	/usr/mdec/installboot -v \
	    -s `cat log | grep -v 'Name' | egrep "/cd-dir/${OSREV}/${MACHINE}/boot$$" | cut -d' ' -f1` \
	    -e `cat log | grep -v 'Name' | egrep "/cd-dir/${OSREV}/${MACHINE}/boot$$" | cut -d' ' -f2` \
	    /mnt/${OSREV}/${MACHINE}/boot /usr/mdec/bootxx /dev/rsvnd0c
	#disklabel -w svnd0 fakecdrom "FabBSD/${MACHINE}   "
	umount /mnt
	vnconfig -u svnd0

install:
	cp ${CDROM} ${RELDIR}/

clean cleandir:
	/bin/rm -f ${CDROM}
	rm -rf cd-dir log

unconfig:
	-umount /mnt
	-vnconfig -u svnd0

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
