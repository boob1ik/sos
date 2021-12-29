#ifndef AIPS_H_
#define AIPS_H_

#include <types.h>
#include <soc_memory_map.h>

typedef struct aipstz
{
    vuint32_t MPR[2]; //!< Master Priviledge Registers
    uint32_t _reserved0[14];
    vuint32_t OPACR; //!< Off-Platform Peripheral Access Control Registers
    vuint32_t OPACR1; //!< Off-Platform Peripheral Access Control Registers
    vuint32_t OPACR2; //!< Off-Platform Peripheral Access Control Registers
    vuint32_t OPACR3; //!< Off-Platform Peripheral Access Control Registers
    vuint32_t OPACR4; //!< Off-Platform Peripheral Access Control Registers
} aipstz_t;

#define AIPSTZ1     (*(aipstz_t *) AIPS1_BASE_ADDR)
#define AIPSTZ2     (*(aipstz_t *) AIPS2_BASE_ADDR)


void aips_init();

#endif /* AIPS_H_ */
