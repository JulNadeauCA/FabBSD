/*	$OpenBSD: _atomic_lock.c,v 1.4 2004/02/02 10:05:55 brad Exp $	*/
/*
 * Atomic lock for sparc64
 */
 
#include "spinlock.h"

int
_atomic_lock(volatile _spinlock_lock_t * lock)
{
	_spinlock_lock_t old;

	/*
	 *  "  ldstub  [address], reg_rd
	 * 
	 *  The atomic load-store instructions copy a byte from memory
	 *  into r[rd]m then rewrite the addressed byte in memory to all
	 *  ones [_SPINLOCK_LOCKED]. The operation is performed
	 *  atomically, that is, without allowing intervening interrupts
	 *  or deferred traps. In a multiprocessor system, two or more
	 *  processors executing atomic load-store unsigned byte [...]
	 *  addressing the same byte [...] simultaneously are guaranteed
	 *  to execute them in an undefined, but serial order."
	 *    - p101, The SPARC Architecture Manual (version 8) Prentice-Hall
	 *
	 * "LDSTUB loads a byte value from memory to a register and writes
	 *  the value FF_16 into the addressed byte atomically. LDSTUB
	 *  is the classic test-and-set instruction. Like SWAP, it has
	 *  a consensus number of two and so cannot resolve more than
	 *  two contending processes in a wait-free fashion."
	 *    - p129, The SPARC Architecture Manual (version 9) Prentice-Hall
	 *  (See also section J.6 (spinlocks))
	 *
	 * (No change to the condition codes are documented.)
	 */
	__asm__("ldstub %0,%1"
		: "=m" (*lock), "=r" (old)
		: "0" (*lock));

	return (old == _SPINLOCK_LOCKED);
}
