#ifndef LPTMR_H_
#define LPTMR_H_

typedef struct {
    vuint32_t CR;       // Control Register
    vuint32_t SR;       // Status Register
    vuint32_t LR;       // Load Register
    vuint32_t CMPR;     // Compare Register
    vuint32_t CNR;      // Counter Register
} EPIT_t;

#define EPIT1_BASE  (0x20D0000u)

#define EPIT1       ((EPIT_t *)EPIT1_BASE)

#endif /* LPTMR_H_ */
