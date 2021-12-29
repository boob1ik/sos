#include <arch.h>
#include "pl310_l2cc.h"

static uint32_t lvl1_ccsidr_data = 0;
static uint32_t lvl1_ccsidr_instr = 0;

static uint32_t lvl2_ccsidr_unified = 0;

void l1_cache_enable() {
    uint32_t csselr;  // Cache Size Selection Register
    uint32_t ccsidr;  // Cache Size ID Register

    // Branch Predictor тоже включаем здесь, а также любые
    // другие аналогичые настройки для ускорения работы ядра
    write_sctlr(read_sctlr() | BM_SCTLR_Z | BM_SCTLR_I | BM_SCTLR_C);
    write_actlr(read_actlr() | BM_ACTLR_L1);
    barrier();
    csselr = 1; // level 1 instruction cache
    asm volatile ("mcr p15, 2, %[csselr], c0, c0, 0\n\
                  isb\n\
                  mrc p15, 1, %[ccsidr], c0, c0, 0" : [ccsidr] "=r" (ccsidr): [csselr] "r" (csselr));
    lvl1_ccsidr_instr = ccsidr;
    csselr = 0; // level 1 data cache
    asm volatile ("mcr p15, 2, %[csselr], c0, c0, 0\n\
                  isb\n\
                  mrc p15, 1, %[ccsidr], c0, c0, 0" : [ccsidr] "=r" (ccsidr): [csselr] "r" (csselr));
    lvl1_ccsidr_data = ccsidr;
}

void l2_cache_enable() {
    uint32_t csselr;  // Cache Size Selection Register
    uint32_t ccsidr;  // Cache Size ID Register

//    init_pl310_l2cc();
    pl310_l2cc_enable(0x00000001);
    write_actlr(read_actlr() | BM_ACTLR_L2);
    barrier();

    csselr = 1 << 1;
    asm volatile ("mcr p15, 2, %[csselr], c0, c0, 0\n\
                  isb\n\
                  mrc p15, 1, %[ccsidr], c0, c0, 0" : [ccsidr] "=r" (ccsidr): [csselr] "r" (csselr));
    lvl2_ccsidr_unified = ccsidr;
}


//! @brief Check if dcache is enabled or disabled
bool dcache_is_enabled ()
{
    return read_sctlr() & BM_SCTLR_C ? true : false;
}

void dcache_enable ()
{
    uint32_t sctlr = read_sctlr();

    if (!(sctlr & BM_SCTLR_C)) {
        // set  C bit (data caching enable)
        sctlr |= BM_SCTLR_C;
        write_sctlr(sctlr);
        dsb();
    }
}

void dcache_disable ()
{
    write_sctlr(read_sctlr() & ~BM_SCTLR_C);
    dsb();
}

void dcache_invalidate ()
{
    uint32_t wayset;  // wayset parameter
    int num_sets;     // number of sets
    int num_ways;     // number of ways

    // Fill number of sets  and number of ways from csid register  This walues are decremented by 1
    num_ways = (lvl1_ccsidr_data >> 0x03) & 0x3FFu; //((csid& csid_ASSOCIATIVITY_MASK) >> csid_ASSOCIATIVITY_SHIFT)

    // Invalidation all lines (all Sets in all ways)
    while (num_ways >= 0)
    {
        num_sets = (lvl1_ccsidr_data >> 0x0D) & 0x7FFFu; //((csid & csid_NUMSETS_MASK) >> csid_NUMSETS_SHIFT)
        while (num_sets >= 0 )
        {
            wayset = (num_sets << 5u) | (num_ways << 30u); //(num_sets << SETWAY_SET_SHIFT) | (num_sets << 3SETWAY_WAY_SHIFT)
            // invalidate line if we know set and way
            asm volatile("mcr p15, 0, %[wayset], c7, c6, 2" : : [wayset] "r" (wayset));
            num_sets--;
        }
        num_ways--;
    }

    // All Cache, Branch predictor and TLB maintenance operations before followed instruction complete
    dsb();

    // Level 2
    set_pl310_l2cc_InvalByWay(0x000000ff);
    while (get_pl310_l2cc_InvalByWay());
    set_pl310_l2cc_CacheSync(1);
    while (get_pl310_l2cc_CacheSync());
}

void dcache_invalidate_line (const void *addr)
{
    uint32_t line_size = 0;
    uint32_t va;

    // get the cache line size
    line_size = 1 << ((lvl1_ccsidr_data & 0x7) + 4);
    va = (uint32_t) addr & (~(line_size - 1)); //addr & va_VIRTUAL_ADDRESS_MASK

    // Invalidate data cache line by va to PoC (Point of Coherency).
    asm volatile("mcr p15, 0, %[va], c7, c6, 1" : : [va] "r" (va));

    // All Cache, Branch predictor and TLB maintenance operations before followed instruction complete
    dsb();
}

void dcache_invalidate_seg (const void *addr, size_t length)
{
    uint32_t va;
    uint32_t line_size = 0;

    line_size = 1 << ((lvl1_ccsidr_data & 0x7) + 4);
    // align the address with line
    const void *end_addr = (const void *)((uint32_t)addr + length);

    do {
        // Invalidate data cache line by va to PoC (Point of Coherency).
        va = (uint32_t) ((uint32_t)addr & (~(line_size - 1))); //addr & va_VIRTUAL_ADDRESS_MASK
        asm volatile("mcr p15, 0, %[va], c7, c6, 1" : : [va] "r" (va));
        // increment addres to next line and decrement lenght
        addr = (const void *) ((uint32_t)addr + line_size);
    } while (addr < end_addr);

    // All Cache, Branch predictor and TLB maintenance operations before followed instruction complete
    dsb();
}

void dcache_flush ()
{
    uint32_t wayset;  // wayset parameter
    int num_sets; // number of sets
    int num_ways; // number of ways

    // Fill number of sets  and number of ways from csid register  This walues are decremented by 1
    num_ways = (lvl1_ccsidr_data >> 0x03) & 0x3FFu; //((csid& csid_ASSOCIATIVITY_MASK) >> csid_ASSOCIATIVITY_SHIFT`)
    while (num_ways >= 0) {
        num_sets = (lvl1_ccsidr_data >> 0x0D) & 0x7FFFu; //((csid & csid_NUMSETS_MASK)      >> csid_NUMSETS_SHIFT  )
        while (num_sets >= 0 ) {
            wayset = (num_sets << 5u) | (num_ways << 30u); //(num_sets << SETWAY_SET_SHIFT) | (num_ways << 3SETWAY_WAY_SHIFT)
            // FLUSH (clean) line if we know set and way
            asm volatile("mcr p15, 0, %[wayset], c7, c10, 2" : : [wayset] "r" (wayset));
            num_sets--;
        }
        num_ways--;
    }

    // All Cache, Branch predictor and TLB maintenance operations before followed instruction complete
    dsb();

    // Level 2
    set_pl310_l2cc_CleanByWay(0x000000ff);
    while (get_pl310_l2cc_CleanByWay());
    set_pl310_l2cc_CacheSync(1);
    while (get_pl310_l2cc_CacheSync());
}

void dcache_flush_line (const void *addr)
{
    uint32_t line_size = 0;
    uint32_t va;

    line_size = 1 << ((lvl1_ccsidr_data & 0x7) + 4);
    va = (uint32_t) addr & (~(line_size - 1)); //addr & va_VIRTUAL_ADDRESS_MASK

    // Clean data cache line to PoU (Point of Unification) by va.
    asm volatile("mcr p15, 0, %[va], c7, c11, 1" : : [va] "r" (va));

    // All Cache, Branch predictor and TLB maintenance operations before followed instruction complete
    dsb();
}

void dcache_flush_seg (const void *addr, size_t length)
{
    uint32_t va = 0;
    uint32_t line_size = 0;
    const void * end_addr = (const void *)((uint32_t)addr + length);

    line_size = 1 << ((lvl1_ccsidr_data & 0x7) + 4);
    do {
        // Clean data cache line to PoU (Point of Unification) by va.
        va = (uint32_t) ((uint32_t)addr & (~(line_size  - 1))); //addr & va_VIRTUAL_ADDRESS_MASK

        asm volatile("mcr p15, 0, %[va], c7, c11, 1" : : [va] "r" (va));

        // increment addres to next line and decrement lenght
        addr = (const void *) ((uint32_t)addr + line_size);
    } while (addr < end_addr);

    // All Cache, Branch predictor and TLB maintenance operations before followed instruction complete
    dsb();
}

bool icache_is_enabled ()
{
    //! @brief Check if icache is enabled or disabled
    return read_sctlr() & BM_SCTLR_I ? true : false;
}

void icache_enable ()
{
    uint32_t sctlr = read_sctlr(); // System Control Register

    // ignore the operation if I is enabled already
    if (!(sctlr & BM_SCTLR_I)) {
        // set  I bit (instruction caching enable)
        sctlr |= BM_SCTLR_I;

        write_sctlr(sctlr);

        // synchronize context on this processor
        isb();
    }
}

void icache_disable ()
{
    write_sctlr(read_sctlr() & ~BM_SCTLR_I);
    isb();
}

void icache_invalidate ()
{
    const uint32_t sbz = 0x0u;
    asm volatile("mcr p15, 0, %[sbz], c7, c5, 0" : : [sbz] "r" (sbz));
    // synchronize context on this processor
    isb();
}


void icache_invalidate_line(const void * addr)
{
    uint32_t line_size = 0;
    uint32_t va;

    line_size = 1 << ((lvl1_ccsidr_instr & 0x7) + 4);
    va = (uint32_t) addr & (~(line_size - 1)); //addr & va_VIRTUAL_ADDRESS_MASK

    asm volatile("mcr p15, 0, %[va], c7, c5, 1" : : [va] "r" (va));
    // synchronize context on this processor
    isb();
}

void icache_invalidate_seg(const void * addr, size_t length)
{
    uint32_t va;
    uint32_t line_size = 0;
    const void * end_addr = (const void *)((uint32_t)addr + length);

    line_size = 1 << ((lvl1_ccsidr_instr & 0x7) + 4);
    do
    {
        // Clean data cache line to PoC (Point of Coherence) by va.
        va = (uint32_t) ((uint32_t)addr & (~(line_size - 1))); //addr & va_VIRTUAL_ADDRESS_MASK
        asm volatile("mcr p15, 0, %[va], c7, c5, 1" : : [va] "r" (va));
        // increment addres to next line and decrement lenght
        addr = (const void *) ((uint32_t)addr + line_size);
    } while (addr < end_addr);

    // synchronize context on this processor
    isb();
}


//void arm_icache_invalidate_is()
//{
//    uint32_t SBZ = 0x0u;
//
//    _ARM_MCR(15, 0, SBZ, 7, 1, 0);
//
//    // synchronize context on this processor
//    _ARM_ISB();
//}
