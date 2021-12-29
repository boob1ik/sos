#ifndef TARGET_BARRIER_H
#define TARGET_BARRIER_H

//static inline void barrier () { asm volatile ("" : : : "memory"); }

/* Data memory barrier */
static inline void dmb ()
{
    asm volatile("dmb" : : : "memory");
}

/* Data synchronization barrier */
static inline void dsb ()
{
    asm volatile("dsb" : : : "memory");
}

/* Instruction synchronization barrier */
static inline void isb ()
{
    asm volatile("isb" : : : "memory");
}

static inline void barrier ()
{
    dsb();
    isb();
}



#endif
