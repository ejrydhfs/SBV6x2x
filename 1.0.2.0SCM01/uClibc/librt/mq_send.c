/*
 * mq_send.c - functions for sending to message queue.
 */

#include <errno.h>
#include <stddef.h>
#include <sys/syscall.h>
#include <mqueue.h>
#ifdef __UCLIBC_HAS_THREADS_NATIVE__
#include "sysdep-cancel.h"
#else
#define SINGLE_THREAD_P 1
#endif

librt_hidden_proto(mq_timedsend)
#ifdef __NR_mq_timedsend
#define __NR___syscall_mq_timedsend __NR_mq_timedsend
static inline _syscall5(int, __syscall_mq_timedsend, int, mqdes,
			const char *, msg_ptr, size_t, msg_len, unsigned int,
			msg_prio, const void *, abs_timeout);
#endif

/*
 * Add a message to queue. If O_NONBLOCK is set and queue is full, wait
 * for sufficient room in the queue until abs_timeout expires.
 */
int mq_timedsend(mqd_t mqdes, const char *msg_ptr, size_t msg_len,
		 unsigned int msg_prio, const struct timespec *abs_timeout)
{
#ifdef __NR_mq_timedsend
  if (SINGLE_THREAD_P)
    return __syscall_mq_timedsend(mqdes, msg_ptr, msg_len, msg_prio,
                                  abs_timeout);
#ifdef __UCLIBC_HAS_THREADS_NATIVE__
  int oldtype = LIBC_CANCEL_ASYNC ();
  int result = __syscall_mq_timedsend(mqdes, msg_ptr, msg_len, msg_prio,
                                      abs_timeout);
  LIBC_CANCEL_RESET (oldtype);
  return result;
#endif
#else
	errno = ENOSYS;
	return -1;
#endif
}

librt_hidden_def(mq_timedsend)

/* Add a message to queue */
int mq_send(mqd_t mqdes, const char *msg_ptr, size_t msg_len,
	    unsigned int msg_prio)
{
	return mq_timedsend(mqdes, msg_ptr, msg_len, msg_prio, NULL);
}
