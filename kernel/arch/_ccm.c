#include <arch.h>

void ccm_init() {

//    CCM->CCGR0 = 0xFFFFFFFF;
//    CCM->CCGR1 = 0xFFFFFFFF;
//    CCM->CCGR2 = 0xFFFFFFFF;
//    CCM->CCGR3 = 0xFFFFFFFF;
//    CCM->CCGR4 = 0xFFFFFFFF;
//    CCM->CCGR5 = 0xFFFFFFFF;
//    CCM->CCGR6 = 0xFFFFFFFF;

    // EPIT1 (scheduler) clock - enable in all power modes, except STOP
    CCM->CCGR1 |= 0x3000;

    // EPIT2 clock - enable in all power modes, except STOP
    CCM->CCGR1 |= 0xc000;
}

void ccm_lpm_enter(lp_modes_t lp_mode) {

}

void ccm_lpm_set_wakeup_source(unsigned long irq_id, bool doEnable) {

}
