#       $OpenBSD: install.md,v 1.24 2008/06/26 05:42:03 ray Exp $
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

MDXAPERTURE=1
ARCH=ARCH

md_installboot() {
	local _rawdev=/dev/r${1}c

	# use extracted mdec if it exists (may be newer)
	if [ -d /mnt/usr/mdec ]; then
		cp /mnt/usr/mdec/boot /mnt/boot
		/mnt/usr/mdec/installboot -v /mnt/boot /mnt/usr/mdec/bootxx $_rawdev
	elif [ -d /usr/mdec ]; then
		cp /usr/mdec/boot /mnt/boot
		/usr/mdec/installboot -v /mnt/boot /usr/mdec/bootxx $_rawdev
	else
		echo No boot block prototypes found, you must run installboot manually.
	fi
}

md_prep_disklabel() {
	local _disk=$1

	disklabel -W $_disk >/dev/null 2>&1
	disklabel -f /tmp/fstab.$_disk -E $_disk
}

md_congrats() {
}

md_consoleinfo() {
	local _d _u
	integer i=0

	# Set up TTYS array to parallel serial device names _d can assume.
	TTYS[0]=tty0
	TTYS[1]=ttyB

	for _d in com scc; do
		for _u in $(scan_dmesg "/^${_d}\([0-9]\) .*/s//\1/p"); do
			if [[ $_d$_u == $CONSOLE || -z $CONSOLE ]]; then
				CDEV=$_d$_u
				CTTY=${TTYS[i]}$_u
				return
			fi
		done
		i=i+1
	done
}
