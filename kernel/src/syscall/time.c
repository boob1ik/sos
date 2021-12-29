#include <thread.h>

void sc_time (struct thread *thr)
{
    int clock_id = (int) thr->uregs->basic_regs[0];
    kernel_time_t *val = (kernel_time_t *) thr->uregs->basic_regs[1];
    kernel_time_t *newval = (kernel_time_t *) thr->uregs->basic_regs[2];
    int ret = ERR;

    // TODO добавить проверку указателей и структур
    // на попадание в адресное пространство процесса
    if (newval != NULL) {
        ret = time_set(clock_id, newval);
//        if(cpu_get_core_id() == CPU_0) {
//            time_update();
//        }
    }
    if (val != NULL) {
        if (ret != OK) {
            // если была ошибка при установке времени, то сохраняем и возвращаем этот код,
            // но пытаемся получить текущее время если требуется
            time_get(clock_id, val);
        } else {
            ret = time_get(clock_id, val);
        }
    }
    thr->uregs->basic_regs[CPU_REG_0] = ret; // return val
}
