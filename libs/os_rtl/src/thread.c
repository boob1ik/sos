#include <os-libc.h>

struct thread_tls_user *__tls() {
    struct thread_tls_user *tls_addr;
    asm volatile ("str r9, [%0]":: "r" (&tls_addr): "cc");
    return tls_addr;
}

int *__tid() {
    return &__tls()->tid;
}

int *__pid() {
    return &__tls()->pid;
}

int *__errno() {
    return &__tls()->reent._errno;
}

struct _reent *__getreent() {
    return &__tls()->reent;
}
