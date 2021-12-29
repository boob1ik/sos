#ifndef TARGET_TIMER_H_
#define TARGET_TIMER_H_

enum timer_irq {
    SCHEDULER_IRQ_SECONDARY_BASE = SW_INTERRUPT_1,
    SCHEDULER_IRQ_ID = IMX_INT_EPIT1
};

#endif /* TIMER_H_ */
