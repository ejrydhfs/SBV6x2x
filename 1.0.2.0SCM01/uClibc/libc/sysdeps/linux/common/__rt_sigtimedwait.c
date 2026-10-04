/* vi: set sw=4 ts=4: */
/*
 * __rt_sigtimedwait() for uClibc
 *
 * Copyright (C) 2000-2006 Erik Andersen <andersen@uclibc.org>
 *
 * Licensed under the LGPL v2.1, see the file COPYING.LIB in this tarball.
 */

#include <sys/syscall.h>
#include <signal.h>
#define __need_NULL
#include <stddef.h>

libc_hidden_proto(sigwaitinfo)
libc_hidden_proto(sigtimedwait)

#ifdef __NR_rt_sigtimedwait
#define __NR___rt_sigtimedwait __NR_rt_sigtimedwait
static _syscall4(int, __rt_sigtimedwait, const sigset_t *, set, siginfo_t *, info,
		  const struct timespec *, timeout, size_t, setsize);

# ifdef __UCLIBC_HAS_THREADS_NATIVE__
#  include <sysdep-cancel.h>
#  include <string.h>
libc_hidden_proto(memcpy)

static int do_sigtimedwait(const sigset_t *set, siginfo_t *info,
						   const struct timespec *timeout)
{
	int result;

#  ifdef SIGCANCEL
	sigset_t tmpset;

	if (set != NULL && (__builtin_expect (__sigismember (set, SIGCANCEL), 0)
#   ifdef SIGSETXID
		|| __builtin_expect (__sigismember (set, SIGSETXID), 0)
#   endif
		))
	{
		/* Create a temporary mask without the bit for SIGCANCEL set.  */
		// We are not copying more than we have to.
		memcpy (&tmpset, set, _NSIG / 8);
		__sigdelset (&tmpset, SIGCANCEL);
#   ifdef SIGSETXID
		__sigdelset (&tmpset, SIGSETXID);
#   endif
		set = &tmpset;
	}
#  endif

	result = __rt_sigtimedwait(set, info, timeout, _NSIG / 8);

	/* The kernel generates a SI_TKILL code in si_code in case tkill is
	   used.  tkill is transparently used in raise().  Since having
	   SI_TKILL as a code is useful in general we fold the results
	   here.  */
	if (result != -1 && info != NULL && info->si_code == SI_TKILL)
		info->si_code = SI_USER;

	return result;
}

int sigwaitinfo(const sigset_t * set, siginfo_t * info)
{
	if(SINGLE_THREAD_P)
		return do_sigtimedwait(set, info, NULL);

	int oldtype = LIBC_CANCEL_ASYNC();

	int result = do_sigtimedwait(set, info, NULL);

	LIBC_CANCEL_RESET(oldtype);

	return result;
}

int sigtimedwait(const sigset_t * set, siginfo_t * info,
				 const struct timespec *timeout)
{
	if(SINGLE_THREAD_P)
		return do_sigtimedwait(set, info, timeout);

	int oldtype = LIBC_CANCEL_ASYNC();

	int result = do_sigtimedwait(set, info, timeout);

	LIBC_CANCEL_RESET(oldtype);

	return result;
}
# else
int sigwaitinfo(const sigset_t * set, siginfo_t * info)
{
	return __rt_sigtimedwait(set, info, NULL, _NSIG / 8);
}

int sigtimedwait(const sigset_t * set, siginfo_t * info,
				 const struct timespec *timeout)
{
	return __rt_sigtimedwait(set, info, timeout, _NSIG / 8);
}
# endif /* !__UCLIBC_HAS_THREADS_NATIVE__ */
#else
int sigwaitinfo(const sigset_t * set, siginfo_t * info)
{
	if (set == NULL)
		__set_errno(EINVAL);
	else
		__set_errno(ENOSYS);
	return -1;
}

int sigtimedwait(const sigset_t * set, siginfo_t * info,
				 const struct timespec *timeout)
{
	if (set == NULL)
		__set_errno(EINVAL);
	else
		__set_errno(ENOSYS);
	return -1;
}
#endif
libc_hidden_def(sigwaitinfo)
libc_hidden_def(sigtimedwait)
