/*	$OpenBSD: archdep.h,v 1.4 2008/04/09 21:45:26 kurt Exp $	*/

/*
 * Copyright (c) 2004 Michael Shalayeff
 * Copyright (c) 1998 Per Fogelstrom, Opsycon AB
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

#ifndef _HPPA_ARCHDEP_H_
#define _HPPA_ARCHDEP_H_

#define	DL_MALLOC_ALIGN	8	/* Arch constraint or otherwise */

#define	MACHID	EM_PARISC		/* ELF e_machine ID value checked */

#define	RELTYPE	Elf_Rela
#define	RELSIZE	sizeof(Elf_Rela)

#include <sys/mman.h>
#include <elf_abi.h>
#include <machine/reloc.h>
#include "syscall.h"
#include "util.h"

static inline void *
_dl_mmap(void *addr, unsigned int len, unsigned int prot,
	unsigned int flags, int fd, off_t offset)
{
	return((void *)_dl__syscall((quad_t)SYS_mmap, addr, len, prot,
		flags, fd, 0, offset));
}

static inline void *
_dl_mquery(void *addr, unsigned int len, unsigned int prot,
	unsigned int flags, int fd, off_t offset)
{
	return((void *)_dl__syscall((quad_t)SYS_mquery, addr, len, prot,
		flags, fd, 0, offset));
}


static inline void
RELOC_REL(Elf_Rel *r, const Elf_Sym *s, Elf_Addr *p, unsigned long v)
{
	/* HPPA does no REL type relocations */
	_dl_exit(20);
}

/*
 * !!!!! WARNING: THIS CODE CANNOT HANDLE ld.so RELOCATIONS OF THE FORM
 * 0000bde8 R_PARISC_DIR32    .data+0x00000048
 * these can be caused by static intialization foo = &bar;
 * prepare to code around this problem, or fix it here.
 */
static inline void
RELOC_RELA(Elf_RelA *r, const Elf_Sym *s, Elf_Addr *p, unsigned long v)
{
	/* XXX fille out _sl ??? */
	if (ELF_R_TYPE(r->r_info) == RELOC_DIR32) {
		*p = v + r->r_addend;
	} else if (ELF_R_TYPE(r->r_info) == RELOC_IPLT) {
		*p = v + s->st_value + r->r_addend;
	} else if (ELF_R_TYPE(r->r_info) == RELOC_PLABEL32) {
		*p = v + s->st_value + r->r_addend;
	} else {
		_dl_printf("unknown bootstrap relocation\n");
		_dl_exit(6);
	}
}

#define RELOC_GOT(obj, offs)

#define	MD_CALL(sobj, func, arg) \
    hppa_call((arg), (sobj)->dyn.pltgot, (func))

#define	MD_ATEXIT(sobj, sym, func) \
    MD_CALL((sobj), (void (*)())((sobj)->obj_base + (sym)->st_value), &_hppa_dl_dtors)

#define GOT_PERMS PROT_READ

void _hppa_dl_dtors(void);
void hppa_call(void *, Elf_Addr *, void (*)(void));
Elf_Addr _dl_md_plabel(Elf_Addr, Elf_Addr *);

#endif /* _HPPA_ARCHDEP_H_ */
