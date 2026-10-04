/* vi: set sw=4 ts=4: */
/*
 * vfork for uClibc
 * Copyright (C) 2000-2006 by Erik Andersen <andersen@uclibc.org>
 * Licensed under the LGPL v2.1, see the file COPYING.LIB in this tarball.
 */

#include <features.h>

#define _ERRNO_H
#include <bits/errno.h>
#include <sys/syscall.h>

#ifndef SAVE_PID
#define SAVE_PID
#endif

#ifndef RESTORE_PID
#define RESTORE_PID
#endif


#ifdef __NR_fork
.text
.global	__vfork
.hidden	__vfork
.type	__vfork,%function
.align 4

__vfork:

#ifdef __NR_vfork
        SAVE_PID
	DO_CALL (vfork)
        RESTORE_PID
	cmn	r0, #4096
#if defined(__USE_BX__)
	bxcc	lr
#else
	movcc	pc, lr
#endif

	/* Check if vfork even exists.  */
	ldr     r1, =-ENOSYS
	teq     r0, r1
	bne     __error
#endif

	/* If we don't have vfork, use fork.  */
	DO_CALL (fork)
	cmn     r0, #4096

	/* Syscall worked.  Return to child/parent */
#if defined(__USE_BX__)
	bxcc	lr
#else
	movcc   pc, lr
#endif

__error:
	b	__syscall_error

.size __vfork,.-__vfork
weak_alias(__vfork,vfork)
libc_hidden_weak(vfork)
#endif
