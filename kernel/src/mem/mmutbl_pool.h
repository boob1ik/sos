#ifndef MMUTBL_POOL_H_
#define MMUTBL_POOL_H_

#include <os_types.h>
#include "vm.h"

#define MAP_NO_POOL               (-1)

void init_mmutbl_pool ();

int load_to_mmutbl_pool (struct mmap *map);
void light_release_mmutbl_pool (int slot);
void release_mmutbl_pool (int slot);

uint32_t* get_mmutbl_pool (int slot);
size_t get_mmutbl_pool_size ();

#endif
