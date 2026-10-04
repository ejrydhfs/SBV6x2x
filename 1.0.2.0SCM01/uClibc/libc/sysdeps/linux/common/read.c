/* vi: set sw=4 ts=4: */
/*
 * read() for uClibc
 *
 * Copyright (C) 2000-2006 Erik Andersen <andersen@uclibc.org>
 *
 * Licensed under the LGPL v2.1, see the file COPYING.LIB in this tarball.
 */

#include <sys/syscall.h>
#include <unistd.h>

extern __typeof(read) __libc_read;
#ifdef __UCLIBC_HAS_THREADS_NATIVE__
#include "sysdep-cancel.h"
#else
#define SINGLE_THREAD_P 1
#endif

#define __NR___syscall_read __NR_read
                      static inline _syscall3(ssize_t, __syscall_read, int, fd, __ptr_t, buf,
                                              size_t, count);

extern __typeof(read) __libc_read;
                      ssize_t __libc_read(int fd, __ptr_t buf, size_t count)
{
  if (SINGLE_THREAD_P)
    return __syscall_read(fd, buf, count);

 #ifdef __UCLIBC_HAS_THREADS_NATIVE__
  int oldtype = LIBC_CANCEL_ASYNC ();
  ssize_t result = __syscall_read(fd, buf, count);
  LIBC_CANCEL_RESET (oldtype);
  return result;
#endif
}

libc_hidden_proto(read)
weak_alias(__libc_read,read)
libc_hidden_weak(read)
