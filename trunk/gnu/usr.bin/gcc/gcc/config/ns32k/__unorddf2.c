/* Lightweight function to test for ieee unordered comparison
   Copyright (C) 2002
   Free Software Foundation, Inc.
   Contributed by Ian Dall <ian@beware.dropbear.id.au>

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

# define ISNAN(x) (							\
  {									\
    union u { double d; unsigned int i[2]; } *t = (union u *)&(x);	\
    ((t->i[1] & 0x7ff00000) == 0x7ff00000) &&				\
    (t->i[0] != 0 || (t->i[1] & 0xfffff) != 0);				\
  })

int __unorddf2 (double, double);
int __unorddf2 (double a, double b)
{
  return ISNAN(a) || ISNAN(b);
}
