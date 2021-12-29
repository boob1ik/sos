#ifndef ARCH_BARRIER_H
#define ARCH_BARRIER_H

/* Data memory barrier */
static inline void dmb ();

/* Data synchronization barrier */
static inline void dsb ();

/* Instruction synchronization barrier */
static inline void isb ();

/* Common synchronization barrier */
static inline void barrier ();

#endif
