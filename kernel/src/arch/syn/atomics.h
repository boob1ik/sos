#ifndef ARCH_ATOMICS_H__
#define ARCH_ATOMICS_H__
#ifdef __cplusplus
extern "C" {
#endif

static inline void atomic_add(int i, atomic_t *v);
static inline void atomic_sub(int i, atomic_t *v);

static inline int atomic_add_return(int i, atomic_t *v);
static inline int atomic_sub_return(int i, atomic_t *v);

static inline int atomic_inc_unless_return(atomic_t *v, int u);

static inline int atomic_inc_if_zero(atomic_t *v);
static inline int atomic_dec_if_one(atomic_t *v);

static inline int atomic_inc_if_gez_unless(atomic_t *v, int u);
static inline int atomic_dec_if_gz(atomic_t *v);

static inline int atomic_read(atomic_t *v);

static inline int64_t atomic64_read(atomic64_t *v);
static inline void atomic64_set(atomic64_t *v, int64_t i);


#ifdef __cplusplus
}
#endif


#endif // ARCH_ATOMICS_H__
