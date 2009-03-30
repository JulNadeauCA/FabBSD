/* LIB_SPEC appropriate for OpenBSD.  Select the appropriate libc, 
   depending on profiling and threads.  Basically,
   -lc(_r)?(_p)?, select _r for threads, and _p for p or pg.  */
/*  Copyright (C) 2004 Free Software Foundation, Inc.

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

#define OBSD_LIB_SPEC "%{!shared:-lc%{pthread:_r}%{p:_p}%{!p:%{pg:_p}}}"

