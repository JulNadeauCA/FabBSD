/*	$OpenBSD: _atomic_lock.c,v 1.3 2004/02/25 04:10:53 deraadt Exp $	*/

/* David Leonard, <d@csee.uq.edu.au>. Public domain. */

/*
 * Atomic lock for amd64 -- taken from i386 code.
 */

#include "spinlock.h"

int
_atomic_lock(volatile _spinlock_lock_t *lock)
{
	_spinlock_lock_t old;

	/*
	 * Use the eXCHanGe instruction to swap the lock value with
	 * a local variable containing the locked state.
	 */
	old = _SPINLOCK_LOCKED;
	__asm__("xchg %0,%1"
		: "=r" (old), "=m" (*lock)
		: "0"  (old), "1"  (*lock));

	return (old != _SPINLOCK_UNLOCKED);
}
