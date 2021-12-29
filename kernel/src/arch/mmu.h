#ifndef _MMU_H_
#define _MMU_H_

void mmu_init ();

void mmu_enable ();
void mmu_disable ();

uint32_t* mmu_get_tbl ();
void mmu_switch (uint32_t *tbl);

static inline void flush_tlb ();

void mmu_set_fault (uint32_t *entry);
void mmu_set_dir (uint32_t *entry, uint32_t va, mem_attributes_t attr);
void mmu_set_pgt (uint32_t *entry, uint32_t *pgt);
void mmu_set_pgte (uint32_t *entry, uint32_t va, mem_attributes_t attr);

const size_t mmu_dirtable_size ();
const size_t mmu_pagetable_size ();
const size_t mmu_page_in_dir ();
const size_t mmu_page_size ();
const size_t mmu_dir_size ();

bool mmu_is_dir (uint32_t *entry);
uint32_t mmu_get_base_addr_dir (uint32_t *entry);

#endif
