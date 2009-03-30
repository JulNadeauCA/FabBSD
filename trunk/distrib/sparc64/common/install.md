#	$OpenBSD: install.md,v 1.22 2008/06/26 05:42:04 ray Exp $
#	$NetBSD: install.md,v 1.3.2.5 1996/08/26 15:45:28 gwr Exp $
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

MDTERM=sun
MDXAPERTURE=1
ARCH=ARCH

md_installboot() {
	local _rawdev=/dev/r${1}c _prefix

	# use extracted mdec if it exists (may be newer)
	if [ -e /mnt/usr/mdec/bootblk ]; then
		_prefix=/mnt/usr/mdec
	elif [ -e /usr/mdec/bootblk ]; then
		_prefix=/usr/mdec
	else
		echo No boot block prototypes found, you must run installboot manually.
		return
	fi

	echo Installing boot block...
	${_prefix}/installboot -v ${_prefix}/bootblk ${_rawdev}
	sync; sync; sync

	if [ -e /mnt/usr/mdec/ofwboot ]; then
		_prefix=/mnt/usr/mdec
	elif [ -e /usr/mdec/ofwboot ]; then
		_prefix=/usr/mdec
	else
		echo No ofwboot found!
		return
	fi
	echo Copying ofwboot...
	cp ${_prefix}/ofwboot /mnt/ofwboot
}

md_prep_disklabel() {
	local _disk=$1

	disklabel -W $_disk >/dev/null 2>&1
	disklabel -f /tmp/fstab.$_disk -E $_disk
}

md_congrats() {
}

md_consoleinfo() {
}
