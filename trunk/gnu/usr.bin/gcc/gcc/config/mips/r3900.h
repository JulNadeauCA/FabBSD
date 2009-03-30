/* Definitions of MIPS sub target machine for GNU compiler.
   Toshiba r3900.  You should include mips.h after this.

   Copyright (C) 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997
   Free Software Foundation, Inc.
   Contributed by Gavin Koch (gavin@cygnus.com).

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#define MIPS_CPU_STRING_DEFAULT "r3900"
#define MIPS_ISA_DEFAULT 1

#define MULTILIB_DEFAULTS { MULTILIB_ENDIAN_DEFAULT, "msoft-float" }

/* We use the MIPS EABI by default.  */
#define MIPS_ABI_DEFAULT ABI_EABI

/* By default (if not mips-something-else) produce code for the r3900 */
#define SUBTARGET_CC1_SPEC "\
%{mhard-float:%e-mhard-float not supported} \
%{msingle-float:%{msoft-float: \
  %e-msingle-float and -msoft-float can not both be specified}}"
