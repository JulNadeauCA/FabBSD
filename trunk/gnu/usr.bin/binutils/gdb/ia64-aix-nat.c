/* Low level interface to IA-64 running AIX for GDB, the GNU debugger.

   Copyright 2000, 2001, 2003 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "inferior.h"
#include "target.h"
#include "gdbcore.h"
#include "regcache.h"
#include <sys/procfs.h>

#include "symtab.h"
#include "bfd.h"
#include "symfile.h"
#include "objfiles.h"

#include <sys/types.h>
#include <fcntl.h>
#include "gdb_stat.h"

void
supply_gregset (prgregset_t *gregsetp)
{
  int regi;

  for (regi = IA64_GR0_REGNUM; regi <= IA64_GR31_REGNUM; regi++)
    {
      regcache_raw_supply (current_regcache, regi, 
			   (char *) &(gregsetp->__gpr[regi - IA64_GR0_REGNUM]));
    }

  for (regi = IA64_BR0_REGNUM; regi <= IA64_BR7_REGNUM; regi++)
    {
      regcache_raw_supply (current_regcache, regi, 
			   (char *) &(gregsetp->__br[regi - IA64_BR0_REGNUM]));
    }

  regcache_raw_supply (current_regcache, IA64_PSR_REGNUM,
		       (char *) &(gregsetp->__psr));
  regcache_raw_supply (current_regcache, IA64_IP_REGNUM,
		       (char *) &(gregsetp->__ip));
  regcache_raw_supply (current_regcache, IA64_CFM_REGNUM,
		       (char *) &(gregsetp->__ifs));
  regcache_raw_supply (current_regcache, IA64_RSC_REGNUM,
		       (char *) &(gregsetp->__rsc));
  regcache_raw_supply (current_regcache, IA64_BSP_REGNUM,
		       (char *) &(gregsetp->__bsp));
  regcache_raw_supply (current_regcache, IA64_BSPSTORE_REGNUM,
		       (char *) &(gregsetp->__bspstore));
  regcache_raw_supply (current_regcache, IA64_RNAT_REGNUM,
		       (char *) &(gregsetp->__rnat));
  regcache_raw_supply (current_regcache, IA64_PFS_REGNUM,
		       (char *) &(gregsetp->__pfs));
  regcache_raw_supply (current_regcache, IA64_UNAT_REGNUM,
		       (char *) &(gregsetp->__unat));
  regcache_raw_supply (current_regcache, IA64_PR_REGNUM,
		       (char *) &(gregsetp->__preds));
  regcache_raw_supply (current_regcache, IA64_CCV_REGNUM,
		       (char *) &(gregsetp->__ccv));
  regcache_raw_supply (current_regcache, IA64_LC_REGNUM,
		       (char *) &(gregsetp->__lc));
  regcache_raw_supply (current_regcache, IA64_EC_REGNUM,
		       (char *) &(gregsetp->__ec));
  /* FIXME: __nats */
  regcache_raw_supply (current_regcache, IA64_FPSR_REGNUM,
		       (char *) &(gregsetp->__fpsr));

  /* These (for the most part) are pseudo registers and are obtained
     by other means.  Those that aren't are already handled by the
     code above.  */
  for (regi = IA64_GR32_REGNUM; regi <= IA64_GR127_REGNUM; regi++)
    deprecated_register_valid[regi] = 1;
  for (regi = IA64_PR0_REGNUM; regi <= IA64_PR63_REGNUM; regi++)
    deprecated_register_valid[regi] = 1;
  for (regi = IA64_VFP_REGNUM; regi <= NUM_REGS; regi++)
    deprecated_register_valid[regi] = 1;
}

void
fill_gregset (prgregset_t *gregsetp, int regno)
{
  int regi;

#define COPY_REG(_fld_,_regi_) \
  if ((regno == -1) || regno == _regi_) \
    memcpy (&(gregsetp->_fld_), &deprecated_registers[DEPRECATED_REGISTER_BYTE (_regi_)], \
	    register_size (current_gdbarch, _regi_))

  for (regi = IA64_GR0_REGNUM; regi <= IA64_GR31_REGNUM; regi++)
    {
      COPY_REG (__gpr[regi - IA64_GR0_REGNUM], regi);
    }

  for (regi = IA64_BR0_REGNUM; regi <= IA64_BR7_REGNUM; regi++)
    {
      COPY_REG (__br[regi - IA64_BR0_REGNUM], regi);
    }
  COPY_REG (__psr, IA64_PSR_REGNUM);
  COPY_REG (__ip, IA64_IP_REGNUM);
  COPY_REG (__ifs, IA64_CFM_REGNUM);
  COPY_REG (__rsc, IA64_RSC_REGNUM);
  COPY_REG (__bsp, IA64_BSP_REGNUM);

  /* Bad things happen if we don't update both bsp and bspstore at the
     same time.  */
  if (regno == IA64_BSP_REGNUM || regno == -1)
    {
      memcpy (&(gregsetp->__bspstore),
	      &deprecated_registers[DEPRECATED_REGISTER_BYTE (IA64_BSP_REGNUM)],
	      register_size (current_gdbarch, IA64_BSP_REGNUM));
      memcpy (&deprecated_registers[DEPRECATED_REGISTER_BYTE (IA64_BSPSTORE_REGNUM)],
	      &deprecated_registers[DEPRECATED_REGISTER_BYTE (IA64_BSP_REGNUM)],
	      register_size (current_gdbarch, IA64_BSP_REGNUM));
    }

#if 0
  /* We never actually write to bspstore, or we'd have to do the same thing
     here too.  */
  COPY_REG (__bspstore, IA64_BSPSTORE_REGNUM);
#endif
  COPY_REG (__rnat, IA64_RNAT_REGNUM);
  COPY_REG (__pfs, IA64_PFS_REGNUM);
  COPY_REG (__unat, IA64_UNAT_REGNUM);
  COPY_REG (__preds, IA64_PR_REGNUM);
  COPY_REG (__ccv, IA64_CCV_REGNUM);
  COPY_REG (__lc, IA64_LC_REGNUM);
  COPY_REG (__ec, IA64_EC_REGNUM);
  /* FIXME: __nats */
  COPY_REG (__fpsr, IA64_FPSR_REGNUM);
#undef COPY_REG
}

void
supply_fpregset (prfpregset_t *fpregsetp)
{
  int regi;

  for (regi = IA64_FR0_REGNUM; regi <= IA64_FR127_REGNUM; regi++)
    regcache_raw_supply (current_regcache, regi, 
                     (char *) &(fpregsetp->__fpr[regi - IA64_FR0_REGNUM]));
}

void
fill_fpregset (prfpregset_t *fpregsetp, int regno)
{
  int regi;
  char *to;
  char *from;

  for (regi = IA64_FR0_REGNUM; regi <= IA64_FR127_REGNUM; regi++)
    {
      if ((regno == -1) || (regno == regi))
	{
	  from = (char *) &deprecated_registers[DEPRECATED_REGISTER_BYTE (regi)];
	  to = (char *) &(fpregsetp->__fpr[regi - IA64_FR0_REGNUM]);
	  memcpy (to, from, register_size (current_gdbarch, regi));
	}
    }
}
