// -*- C++ -*-
/* Copyright (C) 1992 Free Software Foundation, Inc.
     Written by James Clark (jjc@jclark.com)

This file is part of groff.

groff is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

groff is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with groff; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

#include <sys/types.h>
#include <sys/stat.h>

#ifdef HAVE_CC_OSFCN_H
#include <osfcn.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#ifndef S_IRUSR
#define S_IRUSR 0400
#endif

#ifndef S_IRGRP
#define S_IRGRP 0040
#endif

#ifndef S_IROTH
#define S_IROTH 0004
#endif

#ifndef S_ISREG
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif

#ifndef O_RDONLY
#define O_RDONLY 0
#endif
