#	$FabBSD$
#	$OpenBSD: Makefile,v 1.5 2005/03/11 15:40:58 deraadt Exp $

TOP=	${.CURDIR}/..

.include "${TOP}/Makefile.inc"

BSDRD=	bsd.rd
CDROM=	cd${REV}.iso

all: ${CDROM}

${CDROM}:
	-rm -rf ${.OBJDIR}/cd-dir
	mkdir -p ${.OBJDIR}/cd-dir/${OSREV}/amd64
	mkdir -p ${.OBJDIR}/cd-dir/etc
	echo "set image /${OSREV}/amd64/bsd.rd" > ${.OBJDIR}/cd-dir/etc/boot.conf
	cp ${.OBJDIR}/../ramdisk_cd/${BSDRD} ${.OBJDIR}/cd-dir/${OSREV}/amd64
	cp ${DESTDIR}/usr/mdec/cdbr ${.OBJDIR}/cd-dir/${OSREV}/amd64
	cp ${DESTDIR}/usr/mdec/cdboot ${.OBJDIR}/cd-dir/${OSREV}/amd64/cdboot
	mkhybrid -a -R -T -L -l -d -D -N -o ${.OBJDIR}/${CDROM} -v -v \
	    -A "FabBSD ${OSREV} amd64 bootonly CD" \
	    -P "Copyright (c) `date +%Y` Hypertriton, Inc." \
	    -p "Julien Nadeau <vedge@hypertriton.com>" \
	    -V "FabBSD/amd64   ${OSREV} boot-only CD" \
	    -b ${OSREV}/amd64/cdbr -c ${OSREV}/amd64/boot.catalog \
	    ${.OBJDIR}/cd-dir

install:
	cp ${CDROM} ${DESTDIR}/snapshot

clean cleandir:
	/bin/rm -f ${CDROM}
	rm -rf cd-dir

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
