#	$FabBSD$
#	$OpenBSD: install.md,v 1.34 2008/06/26 05:42:03 ray Exp $
#
#
# Copyright (c) 1996 The NetBSD Foundation, Inc.
# All rights reserved.
#
# This code is derived from software contributed to The NetBSD Foundation
# by Jason R. Thorpe.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
# ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
#
# machine dependent section of installation/upgrade script.
#

MDXAPERTURE=2
ARCH=ARCH

md_installboot() {
	local _disk=$1

	[[ $disklabeltype == MBR ]] || return

	echo -n "Copying 'ofwboot' to the boot partition (${_disk}i)..."
	if mount -t msdos /dev/${_disk}i /mnt2 ; then
		if cp /usr/mdec/ofwboot /mnt2; then
			umount /mnt2
			echo "done."
			return
		fi
	fi

	echo "FAILED.\nYou will not be able to boot FabBSD from $_disk."
	exit
}

md_prep_disk() {
	local _disk=$1 _resp
	typeset -l _resp

	cat <<__EOT

$_disk must be partitioned using an HFS or an MBR partition table.

HFS partition tables are created with MacOS. Existing partitions in the HFS
partition table can be modified by FabBSD for use by FabBSD.

MBR partition tables are created with FabBSD. MacOS *cannot* be booted from a
disk partitioned with an MBR partition table.

__EOT

	while :; do
		ask "Use HFS or MBR partition table?" HFS
		_resp=$resp
		case $_resp in
		m|mbr)	export disklabeltype=MBR
			md_prep_MBR $_disk || continue
			disklabel $_disk 2>/dev/null | grep -q "^  i:" || \
				disklabel -w -d $_disk
			newfs -t msdos ${_disk}i
			break
			;;
		h|hfs)	export disklabeltype=HFS
			md_prep_HFS $_disk
			break
			;;
		esac
	done
}

md_prep_MBR() {
	local _disk=$1

	if [[ -n $(disklabel -c $_disk 2>/dev/null | grep ' HFS ') ]]; then
		cat <<__EOT

WARNING: putting an MBR partition table on $_disk will DESTROY the existing HFS
         partitions and HFS partition table.

__EOT
		ask_yn "Are you *sure* you want an MBR partition table on $_disk?"
		[[ $resp == n ]] && exit
	fi

	ask_yn "Use *all* of $_disk for FabBSD?"
	if [[ $resp == y ]]; then
		echo -n "Creating Master Boot Record (MBR)..."
		fdisk -e $_disk  >/dev/null 2>&1 <<__EOT
reinit
update
write
quit
__EOT
		echo "done."
	else
		fdisk $_disk ; fdisk -e $_disk
		fdisk $_disk | grep -q "^..: 06 " || \
			{ echo "No DOS (id 06) partition" ; return 1 ; }
		fdisk $_disk | grep -q "^..: A6 " || \
			{ echo "No FabBSD (id A6) partition" ; return 1 ; }
	fi

	return 0
}

md_prep_HFS() {
	local _disk=$1

	cat <<__EOT

You must modify an existing partition to be of type "FabBSD" and have the name
"FabBSD". If the partition does not exist you must create it with the Apple
MacOS tools before attempting to install FabBSD.

__EOT

	pdisk /dev/${_disk}c
}

md_prep_disklabel() {
	local _disk=$1 _q

	md_prep_disk $_disk

	case $disklabeltype in
	HFS)	;;
	MBR)	cat <<__EOT

You *MUST* setup the FabBSD disklabel to include the MSDOS-formatted boot
partition as the 'i' partition. If the 'i' partition is missing or not the
MSDOS-formatted boot partition, then the 'ofwboot' file required to boot
FabBSD cannot be installed.

__EOT
		;;
	*)	echo "Disk label type ('$disklabeltype') is not 'HFS' or 'MBR'."
		exit
		;;
	esac

	disklabel -W $_disk >/dev/null 2>&1
	disklabel -c -f /tmp/fstab.$_disk -E $_disk
}

md_congrats() {
	if [[ $disklabeltype == HFS ]]; then
		cat <<__EOT

To boot FabBSD, the 'ofwboot' program must be present in the first HFS
partition of $ROOTDISK. If it is not currently present you must boot MacOS and
use MacOS tools to copy it from the FabBSD install media. Then reboot the
machine.
__EOT
	fi

	cat <<__EOT

Once the machine has rebooted use Open Firmware to boot into FabBSD, as
described in the INSTALL.$ARCH document. The command to boot FabBSD will
be something like 'boot hd:,ofwboot /bsd'.

__EOT
}

md_consoleinfo() {
	local _u _d=zstty

	for _u in $(scan_dmesg "/^$_d\([0-9]\) .*/s//\1/p"); do
		if [[ $_d$_u == $CONSOLE || -z $CONSOLE ]]; then
			CDEV=$_d$_u
			: ${CSPEED:=56700}
			set -- a b c d e f g h i j
			shift $_u
			CTTY=tty$1
			return
		fi
	done
}
