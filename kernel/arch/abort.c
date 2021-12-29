#include <os_types.h>

struct abt_info {
    uint32_t* pc;
    uint32_t* address;
    const char *type;
    const char *descr;
    bool ext;
    bool wrt;
};
const char *abt_type[2] = { "Prefetch", "Data" };
const char *abt_desc_str[32] = { "Reserved", // 00000
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
struct abt_info abort_info;

void abort_handler(uint32_t type, uint32_t fsr, uint32_t far, uint32_t _pc) {
    abort_info.pc = (uint32_t*) _pc;
    abort_info.type = abt_type[type];
    abort_info.address = (uint32_t*) far;
    abort_info.ext = !!(fsr & (1 << 12));
    abort_info.wrt = !!(fsr & (1 << 11));
    uint32_t descr_idx = fsr & 0x0000000F;
    if (!!(fsr & (1 << 10)))
        descr_idx += 16;
    abort_info.descr = abt_desc_str[descr_idx];

    asm volatile ("bkpt");
}
