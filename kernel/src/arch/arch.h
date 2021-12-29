/** Arch API */

#ifndef ARCH_H
#ifdef __cplusplus
extern "C" {
#endif
#define ARCH_H

#include <config.h>
#include <os_types.h>

#include "types.h"

#include "syn\atomics.h"
#include "syn\nblock.h"
#include "syn\barrier.h"
#include "syn\spinlock.h"

#include "cpu.h"
#include "mmu.h"
#include "timer.h"
#include "interrupt.h"
#include "cache.h"
#include "ccm.h"

#include <hal.h>

#ifdef __cplusplus
}
#endif
#endif
