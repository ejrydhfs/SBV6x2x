/* vi: set sw=4 ts=4: */
/*
 * close() for uClibc
 *
 * Copyright (C) 2000-2006 Erik Andersen <andersen@uclibc.org>
 *
 * Licensed under the LGPL v2.1, see the file COPYING.LIB in this tarball.
 */

#include <sys/syscall.h>
#include <unistd.h>
#ifdef __UCLIBC_HAS_THREADS_NATIVE__
#include "sysdep-cancel.h"
#else
#define SINGLE_THREAD_P 1
#endif

#define __NR___syscall_close __NR_close
static inline _syscall1(int, __syscall_close, int, fd);

extern __typeof(close) __libc_close;
int __libc_close(int fd)
{
	if (SINGLE_THREAD_P)
		return __syscall_close(fd);

#ifdef __UCLIBC_HAS_THREADS_NATIVE__
	int oldtype = LIBC_CANCEL_ASYNC ();
	int result = __syscall_close(fd);
	LIBC_CANCEL_RESET (oldtype);
	return result;
#endif
}

libc_hidden_proto(close)
weak_alias(__libc_close,close)
libc_hidden_weak(close)
