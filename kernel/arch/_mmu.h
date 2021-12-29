#ifndef TARGET_MMU_H_
#define TARGET_MMU_H_

#include <arch.h>

#define PAGE_SIZE_SHIFT       (12)
#define PAGE_SIZE             (0x1000)                  // 4Kb
#define DIR_SIZE              (0x100000)                // 1Mb
#define PAGE_IN_DIR           (DIR_SIZE / PAGE_SIZE)    // 256

/* RGN bits[4:3] indicates the Outer cacheability attributes
   for the memory associated with the translation table walks */
#define ARM_TTBR_OUTER_NC     (0x0) /* Non-cacheable*/
#define ARM_TTBR_OUTER_WBWA   (0x1) /* Outer Write-Back Write-Allocate Cacheable. */
#define ARM_TTBR_OUTER_WT     (0x2) /* Outer Write-Through Cacheable. */
#define ARM_TTBR_OUTER_WBNWA  (0x3) /* Outer Write-Back no Write-Allocate Cacheable. */

#define ARM_TTBR_OUTER_CACHED ARM_TTBR_OUTER_WBWA
#define ARM_TTBR_FLAGS_OUTER_CACHED (ARM_TTBR_OUTER_CACHED << 3)

#define ARM_TTBR_INNER_CACHED ARM_TTBR_OUTER_NC
#define ARM_TTBR_FLAGS_INNER_CACHED (((ARM_TTBR_INNER_CACHED << 6) & (0x00000040)) | (ARM_TTBR_INNER_CACHED & 0x00000001))

#define ARM_TTBR_ADDR_MASK    (0xffffc000) /* only the 18 upper bits are to be used as address */
#define ARM_TTBR_FLAGS_CACHED ARM_TTBR_FLAGS_OUTER_CACHED | ARM_TTBR_FLAGS_INNER_CACHED


/* Invalidate entire unified TLB */
static inline void flush_tlb ()
{
    dsb();

    /* Invalidate entire unified TLB */
    asm volatile("mcr p15, 0, %[zero], c8, c7, 0" : : [zero] "r" (0));

    /* Invalidate all instruction caches to PoU.
       Also flushes branch target cache. */
    asm volatile("mcr p15, 0, %[zero], c7, c5, 0" : : [zero] "r" (0));

    /* Invalidate entire branch predictor array */
    asm volatile("mcr p15, 0, %[zero], c7, c5, 6" : : [zero] "r" (0)); /* flush BTB */

    dsb();
    isb();
}

#endif
