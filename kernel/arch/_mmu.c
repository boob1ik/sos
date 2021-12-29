/*!
 * @file  mmu.c
 * @brief System memory arangement.
 */
#include <arch.h>

/* In our setup TTBR contains the base address *and* the flags
   but other pieces of the kernel code expect ttbr to be the
   base address of the l1 page table. We therefore add the
   flags here and remove them in the read_ttbr0 */

/* Read Translation Table Base Register 0 */
static inline uint32_t read_ttbr0 ()
{
    uint32_t bar;
    asm volatile("mrc p15, 0, %[bar], c2, c0, 0" : [bar] "=r" (bar));
    return bar & ARM_TTBR_ADDR_MASK;
}

/* Write Translation Table Base Register 0 */
static inline void write_ttbr0 (uint32_t bar)
{
    barrier();

    /* In our setup TTBR contains the base address *and* the flags
       but other pieces of the kernel code expect ttbr to be the
       base address of the l1 page table. We therefore add the
       flags here and remove them in the read_ttbr0 */
    bar = (bar  & ARM_TTBR_ADDR_MASK ) | ARM_TTBR_FLAGS_CACHED;
    asm volatile("mcr p15, 0, %[bar], c2, c0, 0" : : [bar] "r" (bar));
    flush_tlb();
}

/* Reload Translation Table Base Register 0 */
//static inline void reload_ttbr0 ()
//{
//    write_ttbr0(read_ttbr0());
//}

/* Read Translation Table Base Register 1 */
//static inline uint32_t read_ttbr1 ()
//{
//    uint32_t bar;
//    asm volatile("mrc p15, 0, %[bar], c2, c0, 1" : [bar] "=r" (bar));
//    return bar;
//}

/* Write Translation Table Base Register 1 */
//static inline void write_ttbr1 (uint32_t bar)
//{
//    barrier();
//    asm volatile("mcr p15, 0, %[bar], c2, c0, 1" : : [bar] "r" (bar));
//    flush_tlb();
//}

/* Reload Translation Table Base Register 1 */
//static inline void reload_ttbr1 ()
//{
//    uint32_t ttbr = read_ttbr1();
//    write_ttbr1(ttbr);
//}

/* Read Translation Table Base Control Register */
//static inline uint32_t read_ttbcr ()
//{
//    uint32_t bcr;
//    asm volatile("mrc p15, 0, %[bcr], c2, c0, 2" : [bcr] "=r" (bcr));
//    return bcr;
//}

/* Write Translation Table Base Control Register */
static inline void write_ttbcr (uint32_t bcr)
{
    asm volatile("mcr p15, 0, %[bcr], c2, c0, 2" : : [bcr] "r" (bcr));
    isb();
}

/* Read Domain Access Control Register */
//static inline uint32_t read_dacr ()
//{
//    uint32_t dacr;
//    asm volatile("mrc p15, 0, %[dacr], c3, c0, 0" : [dacr] "=r" (dacr));
//    return dacr;
//}

/* Write Domain Access Control Register */
static inline void write_dacr (uint32_t dacr)
{
    asm volatile("mcr p15, 0, %[dacr], c3, c0, 0" : : [dacr] "r" (dacr));
    isb();
}

/** Size in bytes of the 1-level page table.
 The page table must be aligned to a 16Kb address in
 physical memory and could require up to 16Kb of memory.
 */
static const size_t l1_pagetable_size = 0x00004000;

//! Size in bytes of the 2-level page table.
static const size_t l2_pagetable_size = 0x00000400;

static const size_t page_size = 0x1000; // 4 Kb
static const size_t dir_size = 0x100000; // 1 Mb

//! @brief Memory region attributes.
enum mmu_memory_type {
    STRONGLY_ORDERED,
    DEVICE,
    NORMAL,
};

struct memory_type {
    uint32_t tex :3;
    uint32_t c :1;
    uint32_t b :1;
    uint32_t : 27;
};

static struct memory_type get_mem_type (mem_attributes_t attr)
{
    struct memory_type m;
    switch (attr.type) {
    case STRONGLY_ORDERED:
        m.tex = 0;
        m.c = 0;
        m.b = 0;
        break;
    case DEVICE:
        m.c = 0;
        if (attr.shared) {
            m.b = 1;
            m.tex = 0;
        } else {
            m.tex = 2;
            m.b = 0;
        }
        break;
    case NORMAL:
        m.tex = (1 << 2) | attr.outer_cached;
        m.c = attr.inner_cached >> 1;
        m.b = attr.inner_cached;
        break;
    }
    return m;
}

//! @brief Access permissions for a memory region.
enum mmu_access {
    NO_ACCESS,
    PRIVILEGED_ACCESS_ONLY,
    NO_USER_MODE_WRITE,
    FULL_ACCESS,
    RESERVED0,
    PRIVILEGED_READ_ONLY,
    READ_ONLY,
    RESERVED1,
};

union ap_bits {
    enum mmu_access access;
    struct {
        uint32_t ap :2;
        uint32_t apx :1;
        uint32_t : 29;
    };
};

static enum mmu_access get_access (mem_attributes_t attr)
{
    if (attr.process_access > attr.os_access)
        return NO_ACCESS;

    if (attr.process_access == MEM_ACCESS_RW && attr.os_access == MEM_ACCESS_RW)
        return FULL_ACCESS;
    if (attr.process_access == MEM_ACCESS_RO && attr.os_access == MEM_ACCESS_RW)
        return NO_USER_MODE_WRITE;
    if (attr.process_access == MEM_ACCESS_RO && attr.os_access == MEM_ACCESS_RO)
        return READ_ONLY;
    if (attr.process_access == MEM_ACCESS_NO && attr.os_access == MEM_ACCESS_RW)
        return PRIVILEGED_ACCESS_ONLY;
    if (attr.process_access == MEM_ACCESS_NO && attr.os_access == MEM_ACCESS_RO)
        return PRIVILEGED_READ_ONLY;
    if (attr.process_access == MEM_ACCESS_NO && attr.os_access == MEM_ACCESS_NO)
        return NO_ACCESS;
    return NO_ACCESS;
}

//! @brief Level 1 page table entry format
union l1_pte {
    struct {
        uint32_t is_page_table :1;
        uint32_t :4;
        uint32_t domain :4;
        uint32_t parity :1;
        uint32_t base_address :22;
    } page_table;

    struct {
        uint32_t :1; // Should be zero
        uint32_t is_section :1;
        uint32_t bufferable :1;
        uint32_t cacheable :1;
        uint32_t execute_never :1;
        uint32_t domain :4;
        uint32_t :1; // Should be zero
        uint32_t access_permission :2;
        uint32_t tex :3;
        uint32_t access_permission_x :1;
        uint32_t shareable :1;
        uint32_t non_global :1;
        uint32_t :1; // Should be zero
        uint32_t ns:1;
        uint32_t base_address :12;
    } section;

    struct {
        uint32_t :1;
        uint32_t is_section :1;
        uint32_t bufferable :1;
        uint32_t cacheable :1;
        uint32_t execute_never :1;
        uint32_t domain :4;
        uint32_t parity :1;
        uint32_t access_permission :2;
        uint32_t tex :3;
        uint32_t access_permission_x :1;
        uint32_t shareable :1;
        uint32_t non_global :1;
        uint32_t supersection :1;
        uint32_t :5;
        uint32_t base_address :8;
    } supersection;
} __attribute__((packed));

//! @brief Level 2 page table entry format
union l2_pte {
    struct {
        uint32_t _1 :1;  // must be 1
        uint32_t :1;
        uint32_t bufferable :1;
        uint32_t cacheable :1;
        uint32_t access_permission :2;
        uint32_t :3;
        uint32_t access_permission_x :1;
        uint32_t shareable :1;
        uint32_t non_global :1;
        uint32_t tex :3;
        uint32_t execute_never :1;
        uint32_t base_address :16;
    } large_page;

    struct {
        uint32_t execute_never :1;
        uint32_t small :1;
        uint32_t bufferable :1;
        uint32_t cacheable :1;
        uint32_t access_permission :2;
        uint32_t tex :3;
        uint32_t access_permission_x :1;
        uint32_t shareable :1;
        uint32_t non_global :1;
        uint32_t base_address :20;
    } small_page;
} __attribute__((packed));

void mmu_enable ()
{
    flush_tlb();
    write_sctlr(read_sctlr() | BM_SCTLR_M);
}

void mmu_disable ()
{
    write_sctlr(read_sctlr() & ~BM_SCTLR_M);
}

void mmu_switch (uint32_t *tbl)
{
    write_ttbr0((uint32_t)tbl);
}

uint32_t* mmu_get_tbl ()
{
    return (uint32_t*)read_ttbr0();
}

const size_t mmu_dirtable_size ()
{
    return l1_pagetable_size;
}

const size_t mmu_pagetable_size ()
{
    return l2_pagetable_size;
}

void mmu_init ()
{
    write_ttbcr(0);

    // don't use Domains !
    write_dacr(0xFFFFFFFF);
}

void mmu_set_fault (uint32_t *entry)
{
    *entry = 0;
    dcache_flush_line(entry);
}

void mmu_set_dir (uint32_t *entry, uint32_t va, mem_attributes_t attr)
{
    union l1_pte *p = (union l1_pte*)entry;
    p->section.is_section = 1;

    p->section.execute_never = attr.exec;

    union ap_bits ap;
    ap.access = get_access(attr);
    p->section.access_permission = ap.ap;
    p->section.access_permission_x = ap.apx;

    p->section.shareable = attr.shared;

    struct memory_type m = get_mem_type(attr);
    p->section.bufferable = m.b;
    p->section.cacheable = m.c;
    p->section.tex = m.tex;

    p->section.base_address = va >> 20;

    dcache_flush_line(entry);
}

void mmu_set_pgt (uint32_t *entry, uint32_t *pgt)
{
    dcache_flush_seg(pgt, l2_pagetable_size);

    union l1_pte *p = (union l1_pte*)entry;

    p->page_table.is_page_table = 1;
    p->page_table.base_address = (uint32_t)pgt >> 10;
    dcache_flush_line(entry);
}

void mmu_set_pgte (uint32_t *entry, uint32_t va, mem_attributes_t attr)
{
    union l2_pte *p = (union l2_pte*)entry;
    p->small_page.small = 1;

    p->small_page.execute_never = attr.exec;
    p->small_page.shareable = attr.shared;

    union ap_bits ap;
    ap.access = get_access(attr);
    p->small_page.access_permission = ap.ap;
    p->small_page.access_permission_x = ap.apx;

    struct memory_type m = get_mem_type(attr);
    p->small_page.bufferable = m.b;
    p->small_page.cacheable = m.c;
    p->small_page.tex = m.tex;

    p->small_page.base_address = va >> 12;

    dcache_flush_line(entry);
}

const size_t mmu_page_size ()
{
    return page_size;
}

const size_t mmu_dir_size ()
{
    return dir_size;
}

const size_t mmu_page_in_dir ()
{
    return dir_size / page_size;
}

bool mmu_is_dir (uint32_t *entry)
{
    union l1_pte *p = (union l1_pte*)entry;
    return p->section.is_section;
}

uint32_t mmu_get_base_addr_dir (uint32_t *entry)
{
    union l1_pte *l1 = (union l1_pte *)entry;
    if (l1->page_table.is_page_table) {
        return l1->page_table.base_address << 10;
    } else if (l1->section.is_section) {
        if (l1->supersection.supersection)
            return l1->supersection.base_address << 24;
        return l1->section.base_address << 20;
    }

    return 0;
}

mem_attributes_t mmu_get_attr_dir (uint32_t *entry)
{
    mem_attributes_t attr = {0};

    return attr;
}
