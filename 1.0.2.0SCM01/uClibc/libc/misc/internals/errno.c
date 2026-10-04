#include "internal_errno.h"
/* If we're using NPTL, that depends on support for the __thread
   storage class, so we use that for errno and h_errno.  */
#ifdef __UCLIBC_HAS_THREADS_NATIVE__
/* Under NTPL, don't bother with the hidden_proto / hidden_def stuff
   for errno and h_errno: references to TLS variables always require a
   dynamic GOT relocation, even if the symbol is hidden.  */
extern __thread int errno;
extern __thread int h_errno;
__thread int _errno attribute_tls_model_ie;
__thread int _h_errno attribute_tls_model_ie;

weak_tls_alias (_errno, errno);
weak_tls_alias (_h_errno, h_errno);
#else
#ifdef __UCLIBC_HAS_THREADS__
libc_hidden_proto(errno)
libc_hidden_proto(h_errno)
#endif
int errno = 0;
int h_errno = 0;

#ifdef __UCLIBC_HAS_THREADS__
libc_hidden_def(errno)
weak_alias(errno, _errno)
libc_hidden_def(h_errno)
weak_alias(h_errno, _h_errno)
#endif
#endif
