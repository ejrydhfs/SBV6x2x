/*
 * Copyright (C) 2000-2006 Erik Andersen <andersen@uclibc.org>
 *
 * Licensed under the LGPL v2.1, see the file COPYING.LIB in this tarball.
 */

#include <syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>

#ifdef __UCLIBC_HAS_THREADS_NATIVE__
#include "sysdep-cancel.h"
#else
#define SINGLE_THREAD_P 1
#endif

libc_hidden_proto(wait4)

/* Wait for a child to exit.  When one does, put its status in *STAT_LOC and
 * return its process ID.  For errors return (pid_t) -1.  If USAGE is not nil,
 * store information about the child's resource usage (as a `struct rusage')
 * there.  If the WUNTRACED bit is set in OPTIONS, return status for stopped
 * children; otherwise don't.  */
pid_t wait3 (__WAIT_STATUS stat_loc, int options, struct rusage * usage)
{
      if (SINGLE_THREAD_P)
	  return wait4 (WAIT_ANY, stat_loc, options, usage);

#ifdef __UCLIBC_HAS_THREADS_NATIVE__
      int oldtype = LIBC_CANCEL_ASYNC ();
      int result = wait4 (WAIT_ANY, stat_loc, options, usage);
      LIBC_CANCEL_RESET (oldtype);
      return result;
#endif
}
