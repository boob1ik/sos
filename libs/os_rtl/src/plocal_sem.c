#include <os.h>
#include <syn\spinlock.h>
#include <syn\_spinlock.h>
#include <syn\atomics.h>
#include <syn\_atomics.h>

int plocal_sem_wait (syn_t *m, uint64_t timeout) {
    int res;
    if (atomic_dec_if_gz(&m->cnt) == 0) {
        // если успешно захватили
        res = OK;
    } else {
        res = os_syn_wait(m->id, timeout);
    }
    return res;
}

int plocal_sem_done (syn_t *m) {
    int res;
    if (atomic_inc_if_gez_unless(&m->cnt, m->limit) == 0) {
        // выполнена быстрая транзакция освобождения мьютекса в отсутствии ожидающих в очереди
        res = OK;
    } else {
        // очередь заблокированных на мьютексе потоков не пустая, необходимо разблокировать
        // следующий поток в очереди
        res = os_syn_done(m->id);
    }
    return res;
}
