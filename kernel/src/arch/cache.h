#ifndef ARCH_CACHE_H
#define ARCH_CACHE_H

#include <stddef.h>

bool dcache_is_enabled ();
void dcache_invalidate ();
void dcache_invalidate_line (const void *addr);
void dcache_invalidate_seg (const void *addr, size_t length);
void dcache_flush ();
void dcache_flush_line (const void *addr);
void dcache_flush_seg (const void *addr, size_t length);

bool icache_is_enabled ();
void icache_invalidate ();
void icache_invalidate_line (const void *addr);
void icache_invalidate_seg (const void *addr, size_t length);

void l1_cache_enable();
void l2_cache_enable();

#endif
