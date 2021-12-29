#ifndef PTHREAD_EXT_H_
#define PTHREAD_EXT_H_

#define _POSIX_THREADS
#define _POSIX_TIMEOUTS
#define _POSIX_BARRIERS
#define _POSIX_THREAD_PROCESS_SHARED
#define _UNIX98_THREAD_MUTEX_ATTRIBUTES
#define _POSIX_THREAD_PRIO_PROTECT

#include <errno.h>
#include <pthread.h>

int pthread_named_mutex_init (pthread_mutex_t *__mutex, _CONST pthread_mutexattr_t *__attr, char *pathname);
int pthread_named_barrier_init (pthread_barrier_t *__barrier, _CONST pthread_barrierattr_t *__attr,
        unsigned __count, char *pathname);




#endif /* PTHREAD_EXT_H_ */
