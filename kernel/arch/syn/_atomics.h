#ifndef __TARGET_ATOMICS_H__
#define __TARGET_ATOMICS_H__
#ifdef __cplusplus
extern "C" {
#endif

#include <os_types.h>

#ifndef MMU_DISABLED

static inline void atomic_add(int i, atomic_t *v)
{
    unsigned long tmp;
    int result;

    __asm__ __volatile__("@ atomic_add\n"
"1: ldrex   %0, [%3]\n"
"   add %0, %0, %4\n"
"   strex   %1, %0, [%3]\n"
"   teq %1, #0\n"
"   bne 1b"
    : "=&r" (result), "=&r" (tmp), "+Qo" (v->val)
    : "r" (&v->val), "Ir" (i)
    : "cc");
}

static inline void atomic_sub(int i, atomic_t *v)
{
    unsigned long tmp;
    int result;

    __asm__ __volatile__("@ atomic_sub\n"
"1: ldrex   %0, [%3]\n"
"   sub %0, %0, %4\n"
"   strex   %1, %0, [%3]\n"
"   teq %1, #0\n"
"   bne 1b"
    : "=&r" (result), "=&r" (tmp), "+Qo" (v->val)
    : "r" (&v->val), "Ir" (i)
    : "cc");
}

static inline int atomic_add_return(int i, atomic_t *v)
{
    int tmp, value;

    dmb();
    __asm__ __volatile__("@ atomic_add_return\n"
"1: ldrex   %0, [%3]\n"
"   add     %0, %0, %4\n"
"   strex   %1, %0, [%3]\n"
"   teq     %1, #0\n"
"   bne     1b"
    : "=&r" (value), "=&r" (tmp), "+Qo" (v->val)
    : "r" (&v->val), "Ir" (i)
    : "cc");
    dmb();

    return value;
}

static inline int atomic_sub_return(int i, atomic_t *v)
{
    int tmp, value;

    dmb();
    __asm__ __volatile__("@ atomic_sub_return\n"
"1: ldrex   %0, [%3]\n"
"   sub     %0, %0, %4\n"
"   strex   %1, %0, [%3]\n"
"   teq     %1, #0\n"
"   bne     1b"
    : "=&r" (value), "=&r" (tmp), "+Qo" (v->val)
    : "r" (&v->val), "Ir" (i)
    : "cc");
    dmb();

    return value;
}

static inline int atomic_inc_if_zero(atomic_t *v)
{
    int tmp = 1, value;
    dmb();
    __asm__ __volatile__("@ atomic_inc_if_zero\n"
"1: ldrex   %0, [%3]\n"
"   teq     %0, #0\n"
"   bne     2f\n"
"   mov     %0, #1\n"
"   strex   %1, %0, [%3]\n"
"   teq     %1, #0\n"
"   bne     1b\n"
"2:"
    : "=&r" (value), "+r" (tmp), "+Qo" (v->val)
    : "r" (&v->val)
    : "cc");
    if(tmp == 0) {
        dmb();
    }
    return tmp;
}

static inline int atomic_dec_if_one(atomic_t *v)
{
    int tmp = 1, value;
    dmb();
    __asm__ __volatile__("@ atomic_dec_if_one\n"
"1: ldrex   %0, [%3]\n"
"   teq     %0, #1\n"
"   bne     2f\n"
"   mov     %0, #0\n"
"   strex   %1, %0, [%3]\n"
"   teq     %1, #0\n"
"   bne     1b\n"
"2:"
    : "=&r" (value), "+r" (tmp), "+Qo" (v->val)
    : "r" (&v->val)
    : "cc");
    if(tmp == 0) {
        dmb();
    }
    return tmp;
}

static inline int atomic_inc_unless_return(atomic_t *v, int u)
{
    int tmp = 1, value;

    dmb();
    __asm__ __volatile__("@ atomic_inc_unless_return\n"
"1: ldrex   %0, [%3]\n"
"   teq     %0, %4\n"
"   beq     2f\n"
"   add     %0, %0, #1\n"
"   strex   %1, %0, [%3]\n"
"   teq     %1, #0\n"
"   bne     1b\n"
"2:"
    : "=&r" (value), "+r" (tmp), "+Qo" (v->val)
    : "r" (&v->val), "Ir" (u)
    : "cc");
    if(tmp == 0) {
        dmb();
    }

    return value;
}

static inline int atomic_inc_if_gez_unless(atomic_t *v, int u) {
    int tmp = 1, value;
    dmb();
    __asm__ __volatile__("@ atomic_inc_if_gez_unless\n"
"1: ldrex   %0, [%3]\n"
"   cmp     %0, #0\n"
"   blt     2f\n"
"   cmp     %0, %4\n"
"   bge     2f\n"
"   add     %0, %0, #1\n"
"   strex   %1, %0, [%3]\n"
"   teq     %1, #0\n"
"   bne     1b\n"
"2:"
    : "=&r" (value), "+r" (tmp), "+Qo" (v->val)
    : "r" (&v->val), "Ir" (u)
    : "cc");
    if(tmp == 0) {
        dmb();
    }
    return tmp;
}

static inline int atomic_dec_if_gz(atomic_t *v) {
    int tmp = 1, value;
    dmb();
    __asm__ __volatile__("@ atomic_dec_if_gz\n"
"1: ldrex   %0, [%3]\n"
"   cmp     %0, #0\n"
"   ble     2f\n"
"   sub     %0, %0, #1\n"
"   strex   %1, %0, [%3]\n"
"   teq     %1, #0\n"
"   bne     1b\n"
"2:"
    : "=&r" (value), "+r" (tmp), "+Qo" (v->val)
    : "r" (&v->val)
    : "cc");
    if(tmp == 0) {
        dmb();
    }
    return tmp;
}

static inline int atomic_read(atomic_t *v) {
    return v->val;
}


// ВНИМАНИЕ! Данная реализация чтения помечает Shared память для защищенного доступа только
// одним ядром, поэтому в SMP системе необходимо как можно скорее снимать отметку
// записью strexd или другим чтением ldrexd
static inline int64_t atomic64_read(atomic64_t *v)
{
    int64_t result;

    __asm__ __volatile__("@ atomic64_read\n"
"   ldrexd  %0, %H0, [%1]"
    : "=&r" (result)
    : "r" (&v->val), "Qo" (v->val)
    );

    return result;
}

static inline void atomic64_set(atomic64_t *v, int64_t i)
{
    int64_t tmp;

    __asm__ __volatile__("@ atomic64_set\n"
"1: ldrexd  %0, %H0, [%2]\n"
"   strexd  %0, %3, %H3, [%2]\n"
"   teq %0, #0\n"
"   bne 1b"
    : "=&r" (tmp), "=Qo" (v->val)
    : "r" (&v->val), "r" (i)
    : "cc");
}
#else
static inline int64_t atomic64_read(atomic64_t *v) {
    return v->val;
}
static inline void atomic64_set(atomic64_t *v, int64_t i) {
    v->val = i;
}


#endif

#ifdef __cplusplus
}
#endif


#endif // __TARGET_ATOMICS_H__
