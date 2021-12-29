#ifndef LPTMR_H_
#define LPTMR_H_

#include "types.h"

typedef struct {
    vuint32_t CR;       // Control Register
    vuint32_t SR;       // Status Register
    vuint32_t LR;       // Load Register
    vuint32_t CMPR;     // Compare Register
    vuint32_t CNR;      // Counter Register
} EPIT_t;

#define EPIT1_BASE  (0x20D0000u)
#define EPIT2_BASE  (0x20D4000u)

#define EPIT1       ((EPIT_t *)EPIT1_BASE)
#define EPIT2       ((EPIT_t *)EPIT2_BASE)

#define EPIT2_IRQ_ID      (89)

int timer_init(uint64_t period_ns);
int timer_enable();
int timer_disable();
int timer_set_event(uint64_t event_time_ns);
uint64_t systime();
int time_get(int clock_id, kernel_time_t *tv);
int time_set(int clock_id, kernel_time_t *tv);

#endif /* LPTMR_H_ */
