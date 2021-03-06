/*	$OpenBSD: ldasm.S,v 1.4 2004/09/21 09:54:08 pefo Exp $ */

/*
 * Copyright (c) 1998-2002 Opsycon AB, Sweden.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <machine/asm.h>
#include <sys/syscall.h>

/* Stack at this stage is:
 * struct stack {
 *	int	kargc;
 *	char	*kargv[1];	size depends on kargc
 *	char	kargstr[1];	size varies
 *	char	kenvstr[1];	size varies
 * };
 */

FRAMESZ= MKFSIZ(4,16)
GPOFF= FRAMESZ-2*REGSZ
RAOFF= FRAMESZ-1*REGSZ

LEAF(_dl_start, FRAMESZ)		/* Not really LEAF, but we simplify */
	PTR_SUBU sp, FRAMESZ		# Some space.
	SETUP_GP64(GPOFF, _dl_start)

	LA	s1, 1f
	bgezal	zero, 1f
1:
	PTR_SUBU s0, ra, s1		# This is the load offset
	LA	t0, _fdata
	PTR_SRL	t0, 20			# check if distance is > 2**16.
	beqz	t0, 2f
	li	t0, 0x10000

	li	t0, 0x100000
2:

	# This is a hack to change protection of .rodata so it
	# can be relocated. A better way to find the location
	# of .rodata should probably be used.
	# We know that .rodata is aligned on 0x100000 or 0x10000
	# and is at most 64 k in size.
	li	v0, SYS_mprotect
	or	a0, ra, 0xfff
	xor	a0, 0xfff
	PTR_ADDU a0, t0
	li	a1, 0x10000
	li	a2, 7 /* (PROT_READ|PROT_WRITE|PROT_EXEC) */
	syscall

	PTR_ADDU a0, sp, FRAMESZ	# Where stack info is.
	PTR_ADDU a1, sp, 0		# Where fast AUX info will be.
	LA	t9, _dl_boot_bind
	PTR_ADDU t9, s0
	jalr	t9			# Relocate ourself.

	REG_L	a3, FRAMESZ(sp)		# argc
	PTR_ADDU a0, sp, FRAMESZ+REGSZ	# argv
	PTR_ADDU a1, a0, REGSZ
	PTR_SLL  a3, a3, LOGREGSZ
	PTR_ADDU a1, a3
	PTR_ADDU a3, sp, 0		# Where fast AUX info will be.
	move	a2, s0			# Load offset
	jal	_dl_boot		# Go do the linking.

	RESTORE_GP64
	PTR_ADDU sp, FRAMESZ		# Restore stack pointer.
	move	t9, v0			# Entry address from _dl_boot.
	j	t9			# Go execute the 'real' program.
END(_dl_start)

LEAF(_dl__syscall, 0)
	li	v0, SYS___syscall	# Indirect syscall.
	syscall
	bne	a3, zero, 1f
	j	ra
1:
	li	v0, -1
	j	ra
END(_dl__syscall)

	.globl	_dl_rt_resolve
	.ent	_dl_rt_resolve, 0
_dl_rt_resolve:

/* XXX Fix when lazy binding works */
	.end	_dl_rt_resolve
