/* vi: set sw=4 ts=4: */
/*
 * readv() for uClibc
 *
 * Copyright (C) 2000-2006 Erik Andersen <andersen@uclibc.org>
 *
 * Licensed under the LGPL v2.1, see the file COPYING.LIB in this tarball.
 */

#include <sys/syscall.h>
#include <sys/uio.h>
#ifdef __UCLIBC_HAS_THREADS_NATIVE__
#include "sysdep-cancel.h"
#else
#define SINGLE_THREAD_P 1
#endif

extern __typeof(readv) __libc_readv;

#define __NR___syscall_readv __NR_readv
static inline _syscall3(ssize_t, __syscall_readv, int, filedes,
						const struct iovec *, vector, int, count);

ssize_t __libc_readv(int filedes, const struct iovec * vector, int count)
{
	if (SINGLE_THREAD_P)
		return __syscall_readv(filedes, vector, count);

#ifdef __UCLIBC_HAS_THREADS_NATIVE__
	int oldtype = LIBC_CANCEL_ASYNC ();
	ssize_t result = __syscall_readv(filedes, vector, count);
	LIBC_CANCEL_RESET (oldtype);
	return result;
#endif
}

weak_alias(__libc_readv,readv)
