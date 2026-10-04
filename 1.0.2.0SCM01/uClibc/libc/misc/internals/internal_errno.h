/*
 *
 */

#include <features.h>
#include <errno.h>
#include <netdb.h>

#undef errno
#undef h_errno
#ifndef __UCLIBC_HAS_THREADS_NATIVE__
extern int h_errno;
extern int errno;
#ifdef __UCLIBC_HAS_THREADS__
libc_hidden_proto(h_errno)
libc_hidden_proto(errno)
#endif
#endif
