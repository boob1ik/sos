#include <os_types.h>

struct und_info {
    void *pc;
    uint32_t spsr;
    uint32_t opcode; // код команды выровнен на левый крайний бит!
    const char *type;
    const char *descr;
};
const char *und_desc_str[32] = { "Reserved", // 00000
        "Alignment fault", // 00001
        "Debug event", // 00010
        "Section access flag fault", // 00011
        "Instruction cache maintenance fault", // 00100
        "Section translation fault", // 00101
        "Page access flag fault", // 00110
        "Page translation fault", // 00111
        "Synchronous external abort", // 01000
        "Section domain fault", // 01001
        "Reserved", // 01010
        "Page domain fault", // 01011
        "TT walk sync ext abort 1st level", // 01100
        "Section permission fault", // 01101
        "TT walk sync ext abort 2st level", // 01110
        "Page permission fault", // 01111
        "Reserved", // 10000
        "Reserved", // 10001
        "Reserved", // 10010
        "Reserved", // 10011
        "Implementation defined", // 10100
        "Reserved", // 10101
        "Asynchronous external abort", // 10110
        "Reserved", // 10111
        "Memory access async parity error", // 11000
        "Memory access sync parity error", // 11001
        "Implementation defined", // 11010
        "Reserved", // 11011
        "TT walk sync parity error 1st level", // 11100
        "Reserved", // 11101
        "TT walk sync parity error 2st level", // 11110
        "Reserved" // 11111
        };
struct und_info undef_info;

static inline int cp10cp11_enabled() {
    register uint32_t fpexc;
    asm volatile("vmrs    %0,  fpexc         \n\
            ":"=r" (fpexc));
    return fpexc & 0x40000000;
}

extern uint32_t zeroes[16];
static inline void enable_cp10cp11() {
    register uint32_t fpexc;
    register uint32_t *z = zeroes;
    asm volatile("vmrs    %0,  fpexc         \n\
            orr %0, %0, #0x40000000          \n\
            vmsr fpexc, %0                   \n\
            vldm %1, {d0-d7}                 \n\
            vldm %1, {d8-d15}                \n\
            vldm %1, {d16-d23}               \n\
            vldm %1, {d24-d31}               \n\
            isb \n\
            ":"=r" (fpexc): "r" (z));
}

static int ifarm_enable_cp10cp11() {
    if( (undef_info.opcode & 0xf0000000) != 0xf0000000 ) {
        // conditional
        if( ((undef_info.opcode & 0x0c000000) == 0x0c000000) &&
                ((undef_info.opcode & 0x00000e00) == 0x00000a00) ) {
            enable_cp10cp11();
            return 1;
        }
    } else {
        // unconditional
        if( ((undef_info.opcode & 0x08000000) == 0) &&
               ( ((undef_info.opcode & 0x06000000) == 0x02000000) ||
                 ((undef_info.opcode & 0x07100000) == 0x04000000) ) ) {
            // Advanced SIMD
            enable_cp10cp11();
            return 1;
        }
    }
    return 0;
}

static int ifthumb_enable_cp10cp11() {
    if( (((undef_info.opcode & 0xfc000000) == 0xec000000) &&
            ((undef_info.opcode & 0xe00) == 0xa00)) ||
            ((undef_info.opcode & 0xff000000) == 0xf9000000) ) {
        enable_cp10cp11();
        return 1;
    }
    return 0;
}


void undef_handler(uint32_t fsr, uint32_t spsr, void *_pc) {
    uint32_t tmp;
    int cpenabled = cp10cp11_enabled();
    undef_info.pc = _pc;
    undef_info.spsr = spsr;
    if(spsr & 0x20) { // Thumb?
        undef_info.opcode = (*((uint16_t *)_pc)) << 16;
        tmp = undef_info.opcode & 0xf8000000;
        if( (tmp == 0xe8000000) || (tmp == 0xf0000000) ||
                (tmp == 0xf8000000) ) {
            undef_info.opcode |= (*((uint16_t *)(_pc + 2)));
        }
        if(!cpenabled && ifthumb_enable_cp10cp11()) {
            return;
        }
    } else {
        undef_info.opcode = *((uint32_t *)_pc);
        if(!cpenabled && ifarm_enable_cp10cp11()) {
            return;
        }
    }

    uint32_t descr_idx = fsr & 0x0000000F;
    if (!!(fsr & (1 << 10)))
        descr_idx += 16;
    undef_info.descr = und_desc_str[descr_idx];

    asm volatile ("bkpt");
}
