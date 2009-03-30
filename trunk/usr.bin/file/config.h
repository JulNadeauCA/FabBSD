/*
 * Hand-made config.h file for OpenBSD, so we don't have to run
 * the dratted configure script every time we build this puppy,
 * but can still carefully import stuff from Christos' version.
 *
 * This file is in the public domain. Original Author Ian F. Darwin.
 * $OpenBSD: config.h,v 1.3 2008/05/08 01:40:56 chl Exp $
 */

/* header file issues. */
#define HAVE_UNISTD_H 1
#define HAVE_FCNTL_H 1
#define HAVE_SYS_WAIT_H 1
#define HAVE_LOCALE_H 1
#define HAVE_SYS_STAT_H 1
#define	HAVE_INTTYPES_H 1
#define HAVE_GETOPT_H 1
#define HAVE_LIMITS_H 1
/* #define	HAVE_LIBZ 1  DO NOT ENABLE YET -- ian */

#define HAVE_STRTOUL
#define HAVE_STRERROR
#define HAVE_VSNPRINTF
#define HAVE_SNPRINTF

/* Compiler issues */
#define HAVE_LONG_LONG 1
#define SIZEOF_UINT8_T 1
#define SIZEOF_UINT16_T 2
#define SIZEOF_UINT32_T 4
#define SIZEOF_UINT64_T 8

/* Library issues */
#define HAVE_GETOPT_LONG 1	/* in-tree as of 3.2 */
#define HAVE_MKSTEMP 1
#define HAVE_ST_RDEV 1

/* ELF support */
#define BUILTIN_ELF 1
#define ELFCORE 1
