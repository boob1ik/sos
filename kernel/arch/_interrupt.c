#include <arch.h>
#include "gic.h"

static struct interrupt_context ictx[IRQ_NUM];
static int env_priority;
static int min_priority, max_priority, step_priority;
static struct interrupt_context *last_activated;
static int nest_level;

static inline void set_field_vuint32_t(vuint32_t *arg, uint32_t i, uint32_t mask, uint32_t val) {
    *arg = (*arg & (~(mask << i))) | (val << i);
}

void interrupt_init () {
    uint32_t i;
    uint32_t state = interrupt_disable_s();
    if( (GIC_ICC->ICCIIDR !=0x3901243B) || (GIC_ICD->ICDIIDR != 0x0102043B) ) {
        // неверный номер устройства
        while(1);
    }
    if(NUM_CORE != (((GIC_ICD->ICDICTR & 0xe0) >> 5) + 1) ) {
        // количество указанных ядер не соответствует количеству интерфейсов CPU GIC
        while(1);
    }
    last_activated = NULL;
    nest_level = 0;
    min_priority = GIC_PRIORITY_MIN;
    max_priority = GIC_PRIORITY_MAX;
    GIC_ICD->ICDDCR = 0;
    GIC_ICC->ICCICR = 0;
    for(i = 0; i < (IRQ_NUM >> 5); i++) {
        GIC_ICD->ICDISR[i] = 0;             // all interrupts secure
        GIC_ICD->ICDICER[i] = 0xffffffff;   // all disable
        GIC_ICD->ICDICPR[i] = 0xffffffff;
    }

    GIC_ICC->ICCPMR = GIC_PRIORITY_MIN; // current priority - any interrupt enabled
    step_priority = (GIC_PRIORITY_MIN ^ GIC_ICC->ICCPMR) + 1;
    GIC_ICC->ICCBPR = 0;

    for(i = 0; i < IRQ_NUM; i++) {
        ictx[i].id = i;
        ictx[i].nest_level = 0;
        ictx[i].nest_prev = NULL;
        ictx[i].flags.val = 0;
        ictx[i].handler = NULL;
    }
    env_priority = min_priority;
    GIC_ICD->ICDDCR = 1;
    GIC_ICC->ICCICR = 1;
    interrupt_enable_s(state);
}

int interrupt_get_min_prio() {
    return min_priority;
}

int interrupt_get_max_prio() {
    return max_priority;
}

void interrupt_set_env_priority(int priority) {
    env_priority = priority & 0xff;
    GIC_ICC->ICCPMR = env_priority;
}

void interrupt_set_priority(int id, int priority) {
    if( (id < 0) || (id >= IRQ_NUM) ) return;

    // здесь необходимо конвертировать приоритет в шкале приоритетов ОС в приоритет контроллера прерываний,
    // это должно выполняться через линейное отображение(преобразование) диапазонов
    priority = (priority * 4) & 0xff;
    set_field_vuint32_t(&GIC_ICD->ICDIPR[id >> 2], (id & 3) << 3, 0xff, priority & 0xff);
}

void interrupt_set_assert_type (int id, int type) {
    uint32_t val;
    if( (id < 0) || (id >= IRQ_NUM) ) return;
    ictx[id].flags.assert_type = type;
    switch(ictx[id].flags.assert_type) {
    case IRQ_ASSERT_DEFAULT:
    case IRQ_ASSERT_EDGE_RISING:
    case IRQ_ASSERT_EDGE_FALLING:
        val = 2;
        break;
    default:
        val = 0;
        break;
    }
    if(id >= 16) { // if !SGI
        set_field_vuint32_t(&GIC_ICD->ICDICFR[id >> 4], (id & 0xf) << 1, 3, val);
    }
}

int interrupt_hook (int id, void *obj, int type) {
    if( (id < 0) || (id >= IRQ_NUM) ) return ERR_ILLEGAL_ARGS;

    if(ictx[id].handler != NULL) {
        return ERR_BUSY;
    }
    ictx[id].handler = obj;
    ictx[id].nest_level = 0;
    ictx[id].nest_prev = NULL;
    ictx[id].flags.type = type;
    return OK;
}

int interrupt_release (int id) {
    if( (id < 0) || (id >= IRQ_NUM) ) return ERR_ILLEGAL_ARGS;
    interrupt_disable(id);
    ictx[id].handler = NULL;
    return OK;
}

struct interrupt_context *interrupt_get_context (int id) {
    if( (id < 0) || (id >= IRQ_NUM) ) return NULL;
    return &ictx[id];
}

void interrupt_set_cpu(int id, int cpu_id, int enable) {
    if( (id < 32) || (id >= IRQ_NUM) ) return; // первые 32 прерывания - внутренние SGI/PPI
    if( (cpu_id < 0) || (cpu_id >= NUM_CORE) ) return;
    if(enable) {
        GIC_ICD->ICDIPTR[id >> 2] |= (1 << cpu_id) << ((id & 3) << 3);
    } else {
        GIC_ICD->ICDIPTR[id >> 2] &= ~((1 << cpu_id) << ((id & 3) << 3));
    }
}

void interrupt_enable (int id) {
    if( (id < 0) || (id >= IRQ_NUM) ) return;
    GIC_ICD->ICDISER[id >> 5] = 1 << (id & 0x1f);   // enable
}

void interrupt_disable (int id) {
    if( (id < 0) || (id >= IRQ_NUM) ) return;
    GIC_ICD->ICDICER[id >> 5] = 1 << (id & 0x1f);   // disable
}

void interrupt_soft (int id, int cpu_id) {
    if( (id < 0) || (id >= 16) ) return;
    if(cpu_id == -1) {
        GIC_ICD->ICDSGIR = id | (1 << 24);
    } else if(cpu_id < NUM_CORE) {
        GIC_ICD->ICDSGIR = id | ((cpu_id & 0xff) << 16);
    }
}

const struct interrupt_context *interrupt_handle_begin() {
    const struct interrupt_context *res = NULL;
    uint32_t tmp = GIC_ICC->ICCIAR;
    int id = (tmp) & 0x3ff;
    if(id >= 1022) {
        return res;
    }
    if( (id >= IRQ_NUM) || (ictx[id].handler == NULL) || (ictx[id].flags.executing) ) {
        GIC_ICC->ICCEOIR = tmp;
        // @TODO panic
        while(1);
        return NULL;
    }
    ictx[id].tmp = tmp;
    ictx[id].flags.src_core_id = (tmp & (0x7 << 10)) >> 10;
    ictx[id].flags.executing = 1;
    if(last_activated != NULL) {
        nest_level++;
    }
    ictx[id].nest_level = nest_level;
    ictx[id].nest_prev = last_activated;
    last_activated = &ictx[id];
    res = &ictx[id];
    return res;
}

int interrupt_handle_end (int id) {
    if( (id < 0) || (id >= IRQ_NUM) ) return ERR_ILLEGAL_ARGS;
    if(ictx[id].flags.executing == 0) return ERR;

    if( &ictx[id] != last_activated) {
        return ERR;
    }
    if(nest_level > 0) {
        nest_level--;
        last_activated = ictx[id].nest_prev;
    } else {
        last_activated = NULL;
    }
    ictx[id].flags.executing = 0;

    GIC_ICC->ICCEOIR = ictx[id].tmp;
    return OK;
}
