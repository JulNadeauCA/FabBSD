#	$OpenBSD: Makefile,v 1.268 2008/06/15 01:52:19 todd Exp $

TZDIR=		/usr/share/zoneinfo
LOCALTIME=	Canada/Atlantic

NOOBJ=

.if exists(etc.${MACHINE}/Makefile.inc)
.include "etc.${MACHINE}/Makefile.inc"
.endif

# -rw-r--r--
BINOWN= root
BINGRP= wheel
BIN1=	changelist ccd.conf csh.cshrc csh.login csh.logout daily \
	exports ftpusers ftpchroot gettytab group hosts inetd.conf \
	ksh.kshrc man.conf monthly motd mrouted.conf myname \
	netstart networks phones printcap protocols \
	rc rc.conf rc.local rc.securelevel rc.shutdown \
	remote rpc security services shells syslog.conf weekly \
	etc.${MACHINE}/disktab dhclient.conf \
	moduli sensorsd.conf ifstated.conf

.if ${MACHINE} != "aviion" && ${MACHINE} != "landisk" && \
    ${MACHINE} != "mvme68k" && ${MACHINE} != "mvme88k" && \
    ${MACHINE} != "socppc"
BIN1+=	wsconsctl.conf
.endif

# -rw-rw-r--
BIN2=	motd

PCS=	pcs750.bin

# Use NOGZIP on architectures where the gzip'ing would take too much time
# (pmax or slower :-)).  This way you get only tar'ed snap files and you can
# gzip them on a faster machine
.ifndef NOGZIP
GZIPCMD?=	gzip
GZIPFLAGS?=	-9
GZIPEXT?=	.gz
.else
GZIPCMD=	cat
GZIPFLAGS=
GZIPEXT=
.endif

all clean cleandir depend etc install lint:

.ifndef DESTDIR
distribution-etc-root-var distribution distrib-dirs release:
	@echo setenv DESTDIR before doing that!
	@false
.else
distribution-etc-root-var: distrib-dirs
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 ${BIN1} ${DESTDIR}/etc
	ksh ttys.pty | cat etc.${MACHINE}/ttys - > ${DESTDIR}/etc/ttys && \
	    chown ${BINOWN} ${DESTDIR}/etc/ttys && \
	    chgrp ${BINGRP} ${DESTDIR}/etc/ttys && \
	    chmod 644 ${DESTDIR}/etc/ttys
	cat sysctl.conf etc.${MACHINE}/sysctl.conf > ${DESTDIR}/etc/sysctl.conf && \
	    chown ${BINOWN} ${DESTDIR}/etc/sysctl.conf && \
	    chgrp ${BINGRP} ${DESTDIR}/etc/sysctl.conf && \
	    chmod 644 ${DESTDIR}/etc/sysctl.conf
	cat fbtab.head etc.${MACHINE}/fbtab fbtab.tail > ${DESTDIR}/etc/fbtab && \
	    chown ${BINOWN} ${DESTDIR}/etc/fbtab && \
	    chgrp ${BINGRP} ${DESTDIR}/etc/fbtab && \
	    chmod 644 ${DESTDIR}/etc/fbtab
	awk -f ${.CURDIR}/mklogin.conf `test -f etc.${MACHINE}/login.conf.overrides && echo etc.${MACHINE}/login.conf.overrides` < ${.CURDIR}/login.conf.in > \
	    ${DESTDIR}/etc/login.conf && \
	    chown ${BINOWN}:${BINGRP} ${DESTDIR}/etc/login.conf && \
	    chmod 644 ${DESTDIR}/etc/login.conf
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 664 ${BIN2} ${DESTDIR}/etc
	${INSTALL} -c -o root -g wheel -m 600 hosts.equiv ${DESTDIR}/etc
	${INSTALL} -c -o root -g wheel -m 600 crontab ${DESTDIR}/var/cron/tabs/root
	${INSTALL} -c -o root -g wheel -m 600 master.passwd ${DESTDIR}/etc
	pwd_mkdb -p -d ${DESTDIR}/etc /etc/master.passwd
	${INSTALL} -c -o root -g wheel -m 600 snmpd.conf ${DESTDIR}/etc
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 555 \
	    etc.${MACHINE}/MAKEDEV ${DESTDIR}/dev
	cd root; \
		${INSTALL} -c -o root -g wheel -m 644 dot.cshrc \
		    ${DESTDIR}/root/.cshrc; \
		${INSTALL} -c -o root -g wheel -m 644 dot.login \
		    ${DESTDIR}/root/.login; \
		${INSTALL} -c -o root -g wheel -m 644 dot.profile \
		    ${DESTDIR}/root/.profile; \
		rm -f ${DESTDIR}/.cshrc ${DESTDIR}/.profile; \
		${INSTALL} -c -o root -g wheel -m 644 dot.cshrc \
		    ${DESTDIR}/.cshrc; \
		${INSTALL} -c -o root -g wheel -m 644 dot.profile \
		    ${DESTDIR}/.profile
	cd skel; \
		${INSTALL} -c -o root -g wheel -m 644 dot.cshrc \
		    ${DESTDIR}/etc/skel/.cshrc; \
		${INSTALL} -c -o root -g wheel -m 644 dot.login \
		    ${DESTDIR}/etc/skel/.login; \
		${INSTALL} -c -o root -g wheel -m 644 dot.profile \
		    ${DESTDIR}/etc/skel/.profile; \
		${INSTALL} -c -o root -g wheel -m 600 /dev/null \
		    ${DESTDIR}/etc/skel/.ssh/authorized_keys
	cd amd; \
		${INSTALL} -c -o root -g wheel -m 644 master.sample \
		    ${DESTDIR}/etc/amd
	cd mtree; \
		${INSTALL} -c -o root -g wheel -m 600 special \
		    ${DESTDIR}/etc/mtree; \
		${INSTALL} -c -o root -g wheel -m 444 4.4BSD.dist \
		    ${DESTDIR}/etc/mtree; \
		${INSTALL} -c -o root -g wheel -m 444 BSD.local.dist \
		    ${DESTDIR}/etc/mtree; \
		${INSTALL} -c -o root -g wheel -m 444 BSD.x11.dist \
		    ${DESTDIR}/etc/mtree
	/bin/rm -f ${DESTDIR}/etc/localtime
	ln -s ${TZDIR}/${LOCALTIME} ${DESTDIR}/etc/localtime
	${INSTALL} -c -o root -g wheel -m 644 minfree \
	    ${DESTDIR}/var/crash
	${INSTALL} -c -o ${BINOWN} -g operator -m 664 /dev/null \
	    ${DESTDIR}/etc/dumpdates
	${INSTALL} -c -o root -g crontab -m 660 /dev/null \
	    ${DESTDIR}/var/cron/at.deny
	${INSTALL} -c -o root -g crontab -m 660 /dev/null \
	    ${DESTDIR}/var/cron/cron.deny
	${INSTALL} -c -o root -g wheel -m 600 /dev/null \
	    ${DESTDIR}/var/cron/log
	${INSTALL} -c -o ${BINOWN} -g wheel -m 640 /dev/null \
	    ${DESTDIR}/var/log/authlog
	${INSTALL} -c -o ${BINOWN} -g wheel -m 640 /dev/null \
	    ${DESTDIR}/var/log/daemon
	${INSTALL} -c -o ${BINOWN} -g wheel -m 600 /dev/null \
	    ${DESTDIR}/var/log/failedlogin
	${INSTALL} -c -o ${BINOWN} -g wheel -m 640 /dev/null \
	    ${DESTDIR}/var/log/ftpd
	${INSTALL} -c -o ${BINOWN} -g wheel -m 644 /dev/null \
	    ${DESTDIR}/var/log/lastlog
	${INSTALL} -c -o ${BINOWN} -g wheel -m 644 /dev/null \
	    ${DESTDIR}/var/log/messages
	${INSTALL} -c -o ${BINOWN} -g wheel -m 600 /dev/null \
	    ${DESTDIR}/var/log/secure
	${INSTALL} -c -o ${BINOWN} -g wheel -m 644 /dev/null \
	    ${DESTDIR}/var/log/wtmp
	${INSTALL} -c -o ${BINOWN} -g wheel -m 640 /dev/null \
	    ${DESTDIR}/var/log/xferlog
	${INSTALL} -c -o daemon -g staff -m 664 /dev/null \
	    ${DESTDIR}/var/msgs/bounds
	${INSTALL} -c -o ${BINOWN} -g utmp -m 664 /dev/null \
	    ${DESTDIR}/var/run/utmp
.if ${MACHINE} == "vax"
	uudecode -p etc.vax/${PCS}.uu > ${DESTDIR}/${PCS} && \
	    chown ${BINOWN} ${DESTDIR}/${PCS} && \
	    chgrp ${BINGRP} ${DESTDIR}/${PCS} && \
	    chmod 644 ${DESTDIR}/${PCS}
.endif
	cd ../usr.bin/ssh && exec ${MAKE} distribution
	cd ../lib/libssl && exec ${MAKE} distribution
	cd ../gnu/usr.bin/lynx && exec ${MAKE} -f Makefile.bsd-wrapper distribution
	${INSTALL} -c -o root -g wheel -m 440 ../usr.bin/sudo/sudoers \
	    ${DESTDIR}/etc/sudoers

distribution:
	exec ${SUDO} ${MAKE} distribution-etc-root-var
	cd .. && exec ${SUDO} ${MAKE} install

distrib-dirs:
	if [ ! -d ${DESTDIR}/. ]; then \
		${INSTALL} -d -o root -g wheel -m 755 ${DESTDIR}; \
	fi
	mtree -qdef mtree/4.4BSD.dist -p ${DESTDIR}/ -U
	if [ ! -d ${DESTDIR}/usr/src ]; then \
		${INSTALL} -d -o root -g wsrc -m 775 ${DESTDIR}/usr/src; \
	fi
	cd ${DESTDIR}/; rm -f sys; ln -s usr/src/sys sys

.ifndef RELEASEDIR
release:
	@echo setenv RELEASEDIR before building a release.
	@false
.else
release: distribution snap_pre snap_md
	cd ${.CURDIR}/../distrib/notes && ${MAKE} && exec ${SUDO} ${MAKE} install
	cd ${.CURDIR}/../distrib/sets && exec ${SUDO} sh maketars ${OSrev}
	-cp ${DESTDIR}/snapshot/*bsd* ${RELEASEDIR}
	-cp ${DESTDIR}/snapshot/*boot* ${RELEASEDIR}
	-cp ${DESTDIR}/snapshot/cdbr ${RELEASEDIR}
	-cp ${DESTDIR}/snapshot/*BOOT* ${RELEASEDIR}
	-cp ${DESTDIR}/snapshot/cd*.iso ${RELEASEDIR}
	-cp ${DESTDIR}/snapshot/Packages ${RELEASEDIR}
	-cp ${DESTDIR}/snapshot/INSTALL.* ${RELEASEDIR}
	-cp ${DESTDIR}/snapshot/*.fs ${DESTDIR}/snapshot/*.fs.gz ${RELEASEDIR}
	-cd ${RELEASEDIR}; \
		md5 *bsd!(*.gz) *boot* cdbr *BOOT* INSTALL.* Packages *.fs \
		    *.iso *.gz *.tgz > MD5
	-cd ${RELEASEDIR} && sort -o MD5 MD5

snap_pre:
	${SUDO} /bin/rm -rf ${DESTDIR}/snapshot
	${SUDO} ${INSTALL} -d -o root -g wheel -m 755 ${DESTDIR}/snapshot

.endif

.endif	# DESTDIR check

distrib:
	cd ${.CURDIR}/../distrib && \
	    ${MAKE} && exec ${SUDO} ${MAKE} install

DHSIZE=1024 1536 2048 3072 4096
update-moduli:
	( \
		echo -n '#    $Open'; echo 'BSD$'; \
		echo '# Time Type Tests Tries Size Generator Modulus'; \
		( for i in ${DHSIZE}; do \
			ssh-keygen -b $$i -G /dev/stdout; \
		done) | \
		ssh-keygen -T /dev/stdout \
	) > moduli

.PHONY: distribution-etc-root-var distribution distrib-dirs \
	release allarchs snap_md m4 snap_pre

SUBDIR+= etc.alpha etc.amd64 etc.armish etc.i386 etc.macppc etc.sparc
SUBDIR+= etc.sparc64

.include <bsd.subdir.mk>
.include <bsd.prog.mk>
