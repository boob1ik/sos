#include "aips.h"

void aips_init() {
    AIPSTZ1.MPR[0] = 0x77777777;
    if(AIPSTZ1.MPR[0] != 0x77777777) {
        asm volatile ("bkpt");
        return;
    }
    AIPSTZ1.MPR[1] = 0x77777777;
    AIPSTZ1.OPACR = 0;
    AIPSTZ1.OPACR1 = 0;
    AIPSTZ1.OPACR2 = 0;
    AIPSTZ1.OPACR3 = 0;
    AIPSTZ1.OPACR4 = 0;

    AIPSTZ2.MPR[0] = 0x77777777;
    if(AIPSTZ2.MPR[0] != 0x77777777) {
        asm volatile ("bkpt");
        return;
    }
    AIPSTZ2.MPR[1] = 0x77777777;
    AIPSTZ2.OPACR = 0;
    AIPSTZ2.OPACR1 = 0;
    AIPSTZ2.OPACR2 = 0;
    AIPSTZ2.OPACR3 = 0;
    AIPSTZ2.OPACR4 = 0;
}
