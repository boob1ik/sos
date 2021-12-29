#include <os-libc.h>

int pthread_attr_init (pthread_attr_t *__attr)
{
    return OK;
}

int pthread_attr_destroy (pthread_attr_t *__attr)
{
    return OK;
}

int pthread_create (pthread_t *__pthread, _CONST pthread_attr_t *__attr, void *(*__start_routine) (void *), void *__arg)
{
    return OK;
}

int pthread_join (pthread_t __pthread, void **__value_ptr)
{
    return OK;
}

int pthread_detach (pthread_t __pthread)
{
    return OK;
}

void pthread_exit (void *__value_ptr)
{
}

pthread_t pthread_self (void)
{
    return OK;
}

int pthread_equal (pthread_t __t1, pthread_t __t2)
{
    return OK;
}

int pthread_cancel (pthread_t __pthread)
{
    return OK;
}

int pthread_setcancelstate (int __state, int *__oldstate)
{
    return OK;
}

int pthread_setcanceltype (int __type, int *__oldtype)
{
    return OK;
}

//void pthread_cleanup_push (void (*__routine) (void *), void *__arg)
//{
//}
//
//void pthread_cleanup_pop (int __execute)
//{
//}

