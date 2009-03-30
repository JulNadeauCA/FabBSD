#	$FabBSD$
#	$OpenBSD: install.md,v 1.33 2008/06/26 05:42:03 ray Exp $
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
	echo Installing boot block...
	# LBA biosboot uses /boot's i-node number. Using 'cat' preserves that
	# number, so multiboot setups (NTLDR) can work across upgrades.
	cat /usr/mdec/boot >/mnt/boot
	/usr/mdec/installboot -v /mnt/boot /usr/mdec/biosboot ${1}
	echo "done."
}

md_prep_fdisk() {
	local _disk=$1

	ask_yn "Do you want to use *all* of $_disk for FabBSD?"
	if [[ $resp == y ]]; then
		echo -n "Putting all of $_disk into an active FabBSD MBR partition (type 'A6')..."
		fdisk -e ${_disk} <<__EOT >/dev/null
reinit
update
write
quit
__EOT
		echo "done."
		return
	fi

	# Manually configure the MBR.
	cat <<__EOT

You will now create a single MBR partition to contain your FabBSD data. This
partition must have an id of 'A6'; must *NOT* overlap other partitions; and
must be marked as the only active partition.

The 'manual' command describes all the fdisk commands in detail.

$(fdisk ${_disk})
__EOT
	fdisk -e ${_disk}

	cat <<__EOT
Here is the partition information you chose:

$(fdisk ${_disk})
__EOT
}

md_prep_disklabel() {
	local _disk=$1

	md_prep_fdisk $_disk

	cat <<__EOT

You will now create an FabBSD disklabel inside the FabBSD MBR partition.
The disklabel defines how FabBSD splits up the MBR partition into FabBSD
partitions in which filesystems and swap space are created.

The offsets used in the disklabel are ABSOLUTE, i.e. relative to the
start of the disk, NOT the start of the FabBSD MBR partition.

__EOT

	disklabel -W $_disk >/dev/null 2>&1
	disklabel -f /tmp/fstab.$_disk -E $_disk
}

md_congrats() {
}

md_consoleinfo () {
	local _u _d=com

	for _u in $(scan_dmesg "/^$_d\([0-9]\) .*/s//\1/p"); do
		if [[ $_d$_u == $CONSOLE || -z $CONSOLE ]]; then
			CDEV=$_d$_u
			CPROM=com$_u
			CTTY=tty0$_u
			return
		fi
	done
}
