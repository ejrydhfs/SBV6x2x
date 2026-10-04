/*
 * Copyright (C) 2000-2006 Erik Andersen <andersen@uclibc.org>
 *
 * Licensed under the LGPL v2.1, see the file COPYING.LIB in this tarball.
 */
#ifndef _BITS_UCLIBC_ERRNO_H
#define _BITS_UCLIBC_ERRNO_H 1

#ifdef IS_IN_rtld
# undef errno
# define errno _dl_errno
extern int _dl_errno; // attribute_hidden;
#endif /* IS_IN_rtld */

#define __set_errno(val) (errno = (val))

#ifndef __ASSEMBLER__
extern int *__errno_location (void) __THROW __attribute__ ((__const__))
# ifdef IS_IN_rtld
	attribute_hidden
# endif
;
# if defined __UCLIBC_HAS_THREADS_NATIVE__
/* ??? This is a hack because tls.h recursively includes errno.h.
   NPTL implies USE___THREAD.  */
/*libc_hidden_proto(__errno_location)*/
# elif defined __UCLIBC_HAS_THREADS__
#  include <tls.h>
#  if defined USE___THREAD && USE___THREAD
libc_hidden_proto(__errno_location)
#  endif
# endif

#endif /* !__ASSEMBLER__ */

#endif
