#!/bin/sh -
#
# $OpenBSD: sysmerge.sh,v 1.19 2008/07/21 08:28:55 ajacoutot Exp $
#
# This script is based on the FreeBSD mergemaster script, written by
# Douglas Barton <DougB@FreeBSD.org>
#
# Some ideas came from the NetBSD etcupdate script, written by
# Martti Kuparinen <martti@NetBSD.org>
#
# Copyright (c) 1998-2003 Douglas Barton <DougB@FreeBSD.org>
# Copyright (c) 2008 Antoine Jacoutot <ajacoutot@openbsd.org>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

umask 0022

PAGER="${PAGER:=/usr/bin/more}"
SWIDTH=`stty size | awk '{w=$2} END {if (w==0) {w=80} print w}'`
WRKDIR=`mktemp -d -p /var/tmp sysmerge.XXXXX` || exit 1

trap "rm -rf ${WRKDIR}; exit 1" 1 2 3 13 15

if [ -z "${FETCH_CMD}" ]; then
	if [ -z "${FTP_KEEPALIVE}" ]; then
		FTP_KEEPALIVE=0
	fi
	FETCH_CMD="/usr/bin/ftp -V -m -k ${FTP_KEEPALIVE}"
fi


do_pre() {
	if [ -z "${SRCDIR}" -a -z "${TGZ}" -a -z "${XTGZ}" ]; then
		if [ -f "/usr/src/etc/Makefile" ]; then
			SRCDIR=/usr/src
		else
			echo " *** ERROR: please specify a valid path to src or (x)etcXX.tgz"
			exit 1
		fi
	fi

	TEMPROOT="${WRKDIR}/temproot"
	BKPDIR="${WRKDIR}/backups"

	if [ -z "${BATCHMODE}" -a -z "${AUTOMODE}" ]; then
		echo "\n===> Running ${0##*/} with the following settings:\n"
		if [ -n "${TGZURL}" ]; then
			echo " etc source:   ${TGZURL}"
			echo "               (fetched in ${TGZ})"
		elif [ -n "${TGZ}" ]; then
			echo " etc source:   ${TGZ}"
		elif [ -n "${SRCDIR}" ]; then
			echo " etc source:   ${SRCDIR}"
		fi
		if [ -n "${XTGZURL}" ]; then
			echo " xetc source:  ${XTGZURL}"
			echo "               (fetched in ${XTGZ})"
		else
			[ -n "${XTGZ}" ] && echo " xetc source:  ${XTGZ}"
		fi
		echo ""
		echo " base work directory:  ${WRKDIR}"
		echo " temp root directory:  ${TEMPROOT}"
		echo " backup directory:     ${BKPDIR}"
		echo ""
		echo -n "Continue? (y|[n]) "
		read ANSWER
		case "${ANSWER}" in
			y|Y)
				echo ""
				;;
			*)
				rm -rf ${WRKDIR} 2> /dev/null
				exit 1
				;;
		esac
	fi
}


do_populate() {
	echo "===> Creating and populating temporary root under"
	echo "     ${TEMPROOT}"
	mkdir -p ${TEMPROOT}
	if [ "${SRCDIR}" ]; then
		cd ${SRCDIR}/etc
		make DESTDIR=${TEMPROOT} distribution-etc-root-var 2>&1 1> /dev/null \
		  | tee | grep -v "WARNING\: World writable directory"
	fi

	if [ "${TGZ}" -o "${XTGZ}" ]; then
		for i in ${TGZ} ${XTGZ}; do
			tar -xzphf ${i} -C ${TEMPROOT};
		done
	fi

	# files we don't want/need to deal with
	IGNORE_FILES="/etc/*.db /etc/mail/*.db /etc/passwd /etc/motd /etc/myname /var/mail/root"
	CF_FILES="/etc/mail/localhost.cf /etc/mail/sendmail.cf /etc/mail/submit.cf"
	for cf in ${CF_FILES}; do
		CF_DIFF=`diff -q -I "##### " ${TEMPROOT}/${cf} ${DESTDIR}/${cf} 2> /dev/null`
		if [ -z "${CF_DIFF}" ]; then
			IGNORE_FILES="${IGNORE_FILES} ${cf}"
		fi
	done
	if [ -r /etc/sysmerge.ignore ]; then
		while read i; do \
			IGNORE_FILES="${IGNORE_FILES} $(echo ${i} | sed -e 's,\.\.,,g' -e 's,#.*,,g')"
		done < /etc/sysmerge.ignore
	fi
	for i in ${IGNORE_FILES}; do
		rm -rf ${TEMPROOT}/${i};
	done
}


do_install_and_rm() {
	if [ -f "${5}/${4##*/}" ]; then
		mkdir -p ${BKPDIR}/${4%/*}
		cp ${5}/${4##*/} ${BKPDIR}/${4%/*}
	fi

	install -m "${1}" -o "${2}" -g "${3}" "${4}" "${5}"
	rm -f "${4}"
}


mm_install() {
	local INSTDIR
	INSTDIR=${1#.}
	INSTDIR=${INSTDIR%/*}

	if [ -z "${INSTDIR}" ]; then INSTDIR=/; fi

	DIR_MODE=`stat -f "%OMp%OLp" "${TEMPROOT}/${INSTDIR}"`
	eval `stat -f "FILE_MODE=%OMp%OLp FILE_OWN=%Su FILE_GRP=%Sg" ${1}`

	if [ -n "${DESTDIR}${INSTDIR}" -a ! -d "${DESTDIR}${INSTDIR}" ]; then
		install -d -o root -g wheel -m "${DIR_MODE}" "${DESTDIR}${INSTDIR}"
	fi

	do_install_and_rm "${FILE_MODE}" "${FILE_OWN}" "${FILE_GRP}" "${1}" "${DESTDIR}${INSTDIR}"

	case "${1#.}" in
	/dev/MAKEDEV)
		NEED_MAKEDEV=1
		;;
	/etc/login.conf)
		if [ -f ${DESTDIR}/etc/login.conf.db ]; then NEED_CAP_MKDB=1; fi
		;;
	/etc/mail/aliases)
		NEED_NEWALIASES=1
		;;
	/etc/master.passwd)
		NEED_PWD_MKDB=1
		;;
	esac
}


merge_loop() {
	echo "===> Type h at the sdiff prompt (%) to get usage help\n"
	MERGE_AGAIN=1
	while [ "${MERGE_AGAIN}" ]; do
		cp -p "${COMPFILE}" "${COMPFILE}.merged"
		sdiff -as -o "${COMPFILE}.merged" -w ${SWIDTH} \
			"${DESTDIR}${COMPFILE#.}" "${COMPFILE}"
		INSTALL_MERGED=v
		while [ "${INSTALL_MERGED}" = "v" ]; do
			echo ""
			echo "  Use 'i' to install merged file"
			echo "  Use 'r' to re-do the merge"
			echo "  Use 'v' to view the merged file"
			echo "  Default is to leave the temporary file to deal with by hand"
			echo ""
			echo -n "===> How should I deal with the merged file? [Leave it for later] "
			read INSTALL_MERGED
			case "${INSTALL_MERGED}" in
			[iI])
				mv "${COMPFILE}.merged" "${COMPFILE}"
				echo ""
					if mm_install "${COMPFILE}"; then
						echo "===> Merged version of ${COMPFILE} installed successfully"
					else
						echo " *** WARNING: Problem installing ${COMPFILE}, it will remain to merge by hand"
					fi
				unset MERGE_AGAIN
				;;
			[rR])
				rm "${COMPFILE}.merged"
				;;
			[vV])
				${PAGER} "${COMPFILE}.merged"
				;;
			'')
				echo "===> ${COMPFILE} will remain for your consideration"
				unset MERGE_AGAIN
				;;
			*)
				echo "invalid choice: ${INSTALL_MERGED}"
				INSTALL_MERGED=v
				;;
			esac
		done
	done
}


diff_loop() {
	if [ "${BATCHMODE}" ]; then
		HANDLE_COMPFILE=todo
	else
		HANDLE_COMPFILE=v
	fi

	while [ "${HANDLE_COMPFILE}" = "v" -o "${HANDLE_COMPFILE}" = "todo" ]; do
		if [ "${HANDLE_COMPFILE}" = "v" ]; then
			echo "\n========================================================================\n"
		fi
		if [ -f "${DESTDIR}${COMPFILE#.}" -a -f "${COMPFILE}" ]; then
			if [ "${HANDLE_COMPFILE}" = "v" ]; then
				(
					echo "===> Displaying differences between ${COMPFILE} and installed version:"
					echo ""
					diff -u "${DESTDIR}${COMPFILE#.}" "${COMPFILE}"
				) | ${PAGER}
			echo ""
			fi
		else
			echo "===> ${COMPFILE} was not found on the target system"
			if [ -z "${AUTOMODE}" ]; then
				echo ""
				NO_INSTALLED=1
			else
				if mm_install "${COMPFILE}"; then
					echo "===> ${COMPFILE} installed successfully"
					AUTO_INSTALLED_FILES="${AUTO_INSTALLED_FILES}${DESTDIR}${COMPFILE#.}\n"
				else
					echo " *** WARNING: Problem installing ${COMPFILE}, it will remain to merge by hand"
				fi
				return
			fi
		fi

		if [ -z "${BATCHMODE}" ]; then
			echo "  Use 'd' to delete the temporary ${COMPFILE}"
			echo "  Use 'i' to install the temporary ${COMPFILE}"
			if [ -z "${NO_INSTALLED}" -a -z "${IS_BINFILE}" ]; then
				echo "  Use 'm' to merge the temporary and installed versions"
				echo "  Use 'v' to view the diff results again"
			fi
			echo ""
			echo "  Default is to leave the temporary file to deal with by hand"
			echo ""
			echo -n "How should I deal with this? [Leave it for later] "
			read HANDLE_COMPFILE
		else
			unset HANDLE_COMPFILE
		fi

		case "${HANDLE_COMPFILE}" in
		[dD])
			rm "${COMPFILE}"
			echo "\n===> Deleting ${COMPFILE}"
			;;
		[iI])
			echo ""
			if mm_install "${COMPFILE}"; then
				echo "===> ${COMPFILE} installed successfully"
			else
				echo " *** WARNING: Problem installing ${COMPFILE}, it will remain to merge by hand"
			fi
			;;
		[mM])
			if [ -z "${NO_INSTALLED}" -a -z "${IS_BINFILE}" ]; then
				merge_loop
			else
				echo "invalid choice: ${HANDLE_COMPFILE}\n"
				HANDLE_COMPFILE="todo"
			fi
			;;
		[vV])
			if [ -z "${NO_INSTALLED}" -a -z "${IS_BINFILE}" ]; then
				HANDLE_COMPFILE="v"
			else
				echo "invalid choice: ${HANDLE_COMPFILE}\n"
				HANDLE_COMPFILE="todo"
			fi
			;;
		'')
			echo "\n===> ${COMPFILE} will remain for your consideration"
			;;
		*)
			echo "invalid choice: ${HANDLE_COMPFILE}\n"
			HANDLE_COMPFILE="todo"
			continue
			;;
		esac
	done

	unset NO_INSTALLED
}


do_compare() {
	echo "===> Starting comparison"

	cd ${TEMPROOT} || exit 1

	# use -size +0 to avoid comparing empty log files and device nodes
	for COMPFILE in `find . -type f -size +0`; do
		if [ ! -e "${DESTDIR}${COMPFILE#.}" ]; then
			diff_loop
			continue
		fi

		# compare CVS $Id's first so if the file hasn't been modified,
		# it will be deleted from temproot and ignored from comparison.
		# several files are generated from scripts so CVS ID is not a
		# reliable way of detecting changes; leave for a full diff.
		if [ "${AUTOMODE}" -a "${COMPFILE}" != "./etc/fbtab" \
		    -a "${COMPFILE}" != "./etc/login.conf" \
		    -a "${COMPFILE}" != "./etc/sysctl.conf" \
		    -a "${COMPFILE}" != "./etc/ttys" ]; then
			CVSID1=`grep "[$]OpenBSD:" ${DESTDIR}${COMPFILE#.} 2>/dev/null`
			CVSID2=`grep "[$]OpenBSD:" ${COMPFILE} 2>/dev/null` || CVSID2=none
			if [ "${CVSID2}" = "${CVSID1}" ]; then rm "${COMPFILE}"; fi
		fi

		if [ -f "${COMPFILE}" ]; then
			# make sure files are different; if not, delete the one in temproot
			if diff -q "${DESTDIR}${COMPFILE#.}" "${COMPFILE}" > /dev/null 2>&1; then
				rm "${COMPFILE}"
			# xetcXX.tgz contains binary files; set IS_BINFILE to disable sdiff
			elif diff -q "${DESTDIR}${COMPFILE#.}" "${COMPFILE}" | grep "Binary" > /dev/null 2>&1; then
				IS_BINFILE=1
				diff_loop
			else
				unset IS_BINFILE
				diff_loop
			fi
		fi
	done

	echo "\n===> Comparison complete"
}


do_post() {
	if [ "${AUTO_INSTALLED_FILES}" ]; then
		echo "${AUTO_INSTALLED_FILES}" > ${WRKDIR}/auto_installed_files
	fi

	if [ "${NEED_CAP_MKDB}" ]; then
		echo -n "===> You installed a new ${DESTDIR}/etc/login.conf file, "
		if [ "${AUTOMODE}" ]; then
			echo "running cap_mkdb"
			cap_mkdb ${DESTDIR}/etc/login.conf
		else
			echo "\n    rebuild your login.conf database by running the following command as root:"
			echo "    'cap_mkdb ${DESTDIR}/etc/login.conf'"
		fi
	fi

	if [ "${NEED_PWD_MKDB}" ]; then
		echo -n "===> A new ${DESTDIR}/etc/master.passwd file was installed, "
		if [ "${AUTOMODE}" ]; then
			echo "running pwd_mkdb"
			pwd_mkdb -d ${DESTDIR}/etc -p ${DESTDIR}/etc/master.passwd
		else
			echo "\n    rebuild your password files by running the following command as root:"
			echo "    'pwd_mkdb -d ${DESTDIR}/etc -p ${DESTDIR}/etc/master.passwd'"
		fi
	fi

	if [ "${NEED_MAKEDEV}" ]; then
		echo -n "===> A new ${DESTDIR}/dev/MAKEDEV script was installed, "
		if [ "${AUTOMODE}" ]; then
			echo "running MAKEDEV"
			cd ${DESTDIR}/dev && /bin/sh MAKEDEV all
		else
			echo "\n    rebuild your device nodes by running the following command as root:"
			echo "    'cd ${DESTDIR}/dev && /bin/sh MAKEDEV all'"
		fi
	fi

	if [ "${NEED_NEWALIASES}" ]; then
		echo -n "===> A new ${DESTDIR}/etc/mail/aliases file was installed, "
		if [ "${DESTDIR}" ]; then
			echo "\n    but the newaliases command is limited to the directories configured"
			echo "    in sendmail.cf.  Make sure to create your aliases database by"
			echo "    hand when your sendmail configuration is done."
		elif [ "${AUTOMODE}" ]; then
			echo "running newaliases"
			newaliases
		else
			echo "\n    rebuild your aliases database by running the following command as root:"
			echo "    'newaliases'"
		fi
	fi

	# clean leftovers created by make in src
	if [ "${SRCDIR}" ]; then
		cd ${SRCDIR}/gnu/usr.sbin/sendmail/cf/cf && make cleandir 1> /dev/null
	fi

	echo "===> Making sure your directory hierarchy has correct perms, running mtree"
	mtree -qdef ${DESTDIR}/etc/mtree/4.4BSD.dist -p ${DESTDIR:=/} -U 1> /dev/null

	FILES_IN_WRKDIR=`find ${WRKDIR} -type f -size +0 2>/dev/null`
	if [ "${FILES_IN_WRKDIR}" ]; then
		FILES_IN_TEMPROOT=`find ${TEMPROOT} -type f -size +0 2>/dev/null`
		FILES_IN_BKPDIR=`find ${BKPDIR} -type f -size +0 2>/dev/null`
		if [ "${AUTO_INSTALLED_FILES}" ]; then
			echo "===> Automatically installed file(s) listed in"
			echo "     ${WRKDIR}/auto_installed_files"
		fi
		if [ "${FILES_IN_TEMPROOT}" ]; then
			echo "===> File(s) remaining for you to merge by hand:"
			find "${TEMPROOT}" -type f -size +0 -exec echo "     {}" \;
		fi
		if [ "${FILES_IN_BKPDIR}" ]; then
			echo "===> Backup of replaced file(s) can be found under"
			echo "     ${BKPDIR}"
		fi
		echo "===> When done, ${WRKDIR} and its subdirectories should be removed"
	else
		echo "===> Removing ${WRKDIR}"
		rm -rf "${WRKDIR}"
	fi
}


ARGS=`getopt abs:x: $*`
if [ $? -ne 0 ]; then
	echo "usage: ${0##*/} [-ab] [-s src | etcXX.tgz] [-x xetcXX.tgz]" >&2
	exit 1
fi

if [ `id -u` -ne 0 ]; then
	echo " *** ERROR: Need root privilege to run this script"
	exit 1
fi

set -- ${ARGS}
while [ $# -ne 0 ]
do
	case "$1" in
	-a)
		AUTOMODE=1
		shift;;
	-b)
		BATCHMODE=1
		shift;;
	-s)
		WHERE="${2}"
		shift 2
		if [ -f "${WHERE}/etc/Makefile" ]; then
			SRCDIR=${WHERE}
		elif [ -f "${WHERE}" ] && echo -n ${WHERE} |		\
		    awk -F/ '{print $NF}' | 				\
		    grep '^etc[0-9][0-9]\.tgz$' > /dev/null 2>&1 ; then
			TGZ=${WHERE}
		elif echo ${WHERE} | \
		    grep -qE '^(http|ftp)://.*/etc[0-9][0-9]\.tgz$'; then
			TGZ=${WRKDIR}/etc.tgz
			TGZURL="${WHERE}"
			if ! ${FETCH_CMD} -o ${TGZ} ${TGZURL}; then
				echo " *** ERROR: Could not retrieve ${TGZURL}"
				exit 1
			fi
		else
			echo " *** ERROR: ${WHERE} is not a path to src nor etcXX.tgz"
			exit 1
		fi
		;;
	-x)
		WHERE="${2}"
		shift 2
		if [ -f "${WHERE}" ] && echo -n ${WHERE} | 		\
		    awk -F/ '{print $NF}' | 				\
		    grep '^xetc[0-9][0-9]\.tgz$' > /dev/null 2>&1 ; then
			XTGZ=${WHERE}
		elif echo ${WHERE} | \
		    grep -qE '^(http|ftp)://.*/xetc[0-9][0-9]\.tgz$'; then
			XTGZ=${WRKDIR}/xetc.tgz
			XTGZURL="${WHERE}"
			if ! ${FETCH_CMD} -o ${XTGZ} ${XTGZURL}; then
				echo " *** ERROR: Could not retrieve ${XTGZURL}"
				exit 1
			fi
		else
			echo " *** ERROR: ${WHERE} is not a path to xetcXX.tgz"
			exit 1
		fi
		;;
	--)
		shift
		break;;
	esac
done


do_pre
do_populate
do_compare
do_post
