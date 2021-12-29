#ifndef ARCH_CCM_H
#define ARCH_CCM_H

#include <stdint.h>
#include <stdbool.h>

typedef enum _lp_modes {
    RUN_MODE,
    WAIT_MODE,
    STOP_MODE,
} lp_modes_t;

void ccm_init();
void ccm_lpm_enter(lp_modes_t lp_mode);
void ccm_lpm_set_wakeup_source(unsigned long irq_id, bool doEnable);

#endif 
