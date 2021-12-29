#ifndef TARGET_CCM_H_
#define TARGET_CCM_H_

#include <arch\types.h>

/** CCM - Register Layout Typedef */
typedef struct {
    vuint32_t CCR;          //!< CCM Control Register
    vuint32_t CCDR;         //!< CCM Control Divider Register
    vuint32_t CSR;          //!< CCM Status Register
    vuint32_t CCSR;         //!< CCM Clock Swither Register
    vuint32_t CACRR;        //!< CCM Arm Clock Root Register
    vuint32_t CBCDR;        //!< CCM Bus Clock Divider Register
    vuint32_t CBCMR;        //!< CCM Bus Clock Multiplexer Register
    vuint32_t CSCMR1;       //!< CCM Serial Clock Multiplexer Register 1
    vuint32_t CSCMR2;       //!< CCM Serial Clock Multiplexer Register 2
    vuint32_t CSCDR1;       //!< CCM Serial Clock Divider Register 1
    vuint32_t CS1CDR;       //!< CCM SSI1 Clock Divider Register
    vuint32_t CS2CDR;       //!< CCM SSI2 Clock Divider Register
    vuint32_t CDCDR;        //!< CCM D1 Clock Divider Register
    vuint32_t CHSCCDR;      //!< CCM HSC Clock Divider Register
    vuint32_t CSCDR2;       //!< CCM Serial Clock Divider Register 2
    vuint32_t CSCDR3;       //!< CCM Serial Clock Divider Register 3
    vuint32_t _reserved0;
    vuint32_t CWDR;         //!< CCM Wakeup Detector Register
    vuint32_t CDHIPR;       //!< CCM Divider Handshake In-Process Register
    vuint32_t _reserved1;
    vuint32_t CTOR;         //!< CCM Testing Observability Register
    vuint32_t CLPCR;        //!< CCM Low Power Control Register
    vuint32_t CISR;         //!< CCM Interrupt Status Register
    vuint32_t CIMR;         //!< CCM Interrupt Mask Register
    vuint32_t CCOSR;        //!< CCM Clock Output Source Register
    vuint32_t CGPR;         //!< CCM General Purpose Register
    vuint32_t CCGR0;        //!< CCM Clock Gating Register 0
    vuint32_t CCGR1;        //!< CCM Clock Gating Register 1
    vuint32_t CCGR2;        //!< CCM Clock Gating Register 2
    vuint32_t CCGR3;        //!< CCM Clock Gating Register 3
    vuint32_t CCGR4;        //!< CCM Clock Gating Register 4
    vuint32_t CCGR5;        //!< CCM Clock Gating Register 5
    vuint32_t CCGR6;        //!< CCM Clock Gating Register 6
    vuint32_t _reserved2;
    vuint32_t CMEOR;        //!< CCM Module Enable Overide Register
} CCM_regs_t;

/* CCM - Peripheral instance base addresses */
/** Peripheral CCM base address */
#define CCM_BASE                                 (0x20c4000u)
/** Peripheral CCM base pointer */
#define CCM                                      ((CCM_regs_t *)CCM_BASE)



#endif /* CCM_H_ */
