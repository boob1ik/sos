#ifndef UTILS_H
#define UTILS_H

#include <os_types.h>

#define ALIGN(x,a) (((size_t)x + (size_t)(a - 1)) & ~(size_t)(a - 1))

#endif
