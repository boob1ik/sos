#include <proc.h>
#include <mem\vm.h>

// args = (int tid)
void sc_thread_join (struct thread *thr)
{
    // TODO ВНИМАНИЕ!!! Действия связаны с жизненным циклом процессов/потоков и их диспетчеризацией,
    // поэтому нужно особенно внимательно работать по указателям и гарантировать их
    // адекватность в любой момент времени включая другие ядра в SMP
    // путем применения блокировок


    int tid = (int)thr->uregs->basic_regs[0];
    struct thread *join_to;

    // TODO какие-то проверки безопасности если нужно

    thread_allocator_lock();
    join_to = get_thr(tid);
    if(join_to == NULL) {
        // целевой либо отсутствует либо только что завершился на другом ядре
        // значит продолжаем выполняться
        thread_allocator_unlock();
        return;
    }
    thr->state = BLOCKED;
    thr->block.type = JOIN;
    thr->block.object.thr = join_to;
    thr->block_list.next = NULL;
    thr->time_sum = 0;

    thread_lock(join_to);
    if(join_to->last_joined == NULL) {
        join_to->last_joined = thr;
        thr->block_list.prev = NULL;
    } else {
        join_to->last_joined->block_list.next = thr;
        thr->block_list.prev = join_to->last_joined;
        join_to->last_joined = thr;
    }
    thread_unlock(join_to);

    thread_allocator_unlock();
}
