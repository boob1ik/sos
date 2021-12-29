#ifndef THREAD_H_
#define THREAD_H_

#include <os_types.h>

#define __DYNAMIC_REENT__

extern struct thread_tls_user *__tls();
extern int *__tid();
extern int *__pid();
extern int *__errno();
extern struct _reent *__getreent();

#endif /* THREAD_H_ */
