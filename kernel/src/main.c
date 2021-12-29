#include <arch.h>
#include <bsp.h>
#include <common\log.h>
#include "interrupt.h"
#include "mem\kmem.h"
#include "mem\vm.h"
#include "common\syshalt.h"
#include "sched.h"
#include <string.h>
#include "event.h"
#include "syn\syn.h"
#include "ipc/channel.h"
#include <os_types.h>
#include <ipc/connection.h>
#include "common/printf.h"
#include "common/namespace.h"

static void print_banner ()
{
    log_info("\n\rbooting 0 core\n\r");
    log_info("Kernel init\n\r");
}


static void announce ()
{
    log_info("\n\rStarting Secure OS %s\n\r", OS_VERSION);
}

int secondary_main ()
{
    log_info("booting %d core\n\r", cpu_get_core_id());
    vm_enable();
    sched_switch(SCHED_SWITCH_NO_RETURN);
    syshalt(SYSHALT_OOPS_ERROR);
    return -1;
}

static void run_secondary ()
{
#ifdef BUILD_SMP
    for (int i = 1; i < NUM_CORE; i++) {
        ; //cpu_enable_core(i, (void*)secondary_main);
    }
#endif
}

int main ()
{
    kmem_init();
    board_init();
    print_banner();
    vm_init();
    board_mem_init();
    kmap_init();
    vm_enable();
    interrupt_init();
    syn_allocator_init();
    pathname_init();
    kevent_init();
    sched_init();
    board_boot_init();
    announce();
    board_post_init();
    run_secondary();
    sched_start();

    syshalt(SYSHALT_OOPS_ERROR);
    return -1;
}

