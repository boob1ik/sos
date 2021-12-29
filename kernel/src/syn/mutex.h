#ifndef MUTEX_H_
#define MUTEX_H_

#include <os_types.h>
#include <arch\syn\spinlock.h>

/**
 * Мьютекс реализован с помощью универсального объекта ядра mutex_t.
 * В нашей ОС различаем 2 типа мьютексов:
 * 1) Мьютекс, размещенный в памяти ядра ОС.
 * Работа с ним возможна только внутри ядра. Если он создан процессом,
 * это значит что процесс может с ним работать только через системные вызовы.
 * (так называемый разделяемый мьютекс если говорить о POSIX).
 * Такой мьютекс ориентирован больше на межпроцессное взаимодействие,
 * гарантирующее исключительное право захвата и освобождения мьютекса одному потоку в одном
 * блоке синхронизации. Использовать такой мьютекс внутри одного процесса не целесообразно но можно
 * (медленнее), для этого предназначен второй тип мьютекса:
 * 2) Мьютекс, счетчик которого и некоторые др. поля размещены в памяти процесса.
 * Такой мьютекс предназначен для организации взаимодействия между потоками одного процесса.
 * Благодаря размещению счетчика в памяти процесса потоки могут предпринимать попытки захвата
 * и освобождать мьютекс без выполнения системных вызовов. Только в случае неудачного захвата
 * выполняется системный вызов и ядро блокирует поток до успешного захвата мьютекса.
 * В то же время мьютекс второго типа можно применять и для межпроцессной синхронизации,
 * если разрешить доступ к памяти другим процессам. Для этого необходимо выделять
 * отдельную страницу(ы) памяти через системный вызов. Сам процесс передачи выполняется
 * передачей системного сообщения целевым процессам в доступные каналы сообщений, что не всегда
 * возможно и приемлемо.
 *
 * Системный вызов выполняется одинаково для 2-х видов мьютексов, разница только в области применения
 * из-за местонахождения рабочих данных.
 *
 * С мьютексами работаем только в вытесняемом контексте, т.к. это объект длительной синхронизации
 * (с разрешенными прерываниями, обычно всегда в режиме SVC, т.к. работаем с вытеснением в SVC всегда
 *  когда не лочимся на объекте ядра). Следствие: нельзя применять внутри синхронизации по объекту ядра
 */

typedef struct {
//    enum syn_type   type;
    atomic_t        *cnt;           //! ссылка на основной рабочий счетчик
    int             *owner_tid;     //! сслыка на номер текущего потока-захватчика
    spinlock_t      wait_lock;      //! спинлок очереди ожидающих потоков
    struct thread   *wait_first;    //! первый из очереди ожидающих (сама очередь через thread.list.prev/next)
    struct thread   *wait_last;     //! последний из очереди ожидающих
    atomic_t        __cnt;          //! рабочий счетчик для мьютекса, размещенного в памяти ядра MUTEX_TYPE_PSHARED
    int             __owner_tid;    //! номер текущего потока-захватчика, тоже для MUTEX_TYPE_PSHARED
    char            *name;          //! Имя,  MUTEX_TYPE_PLOCAL не имеет имени
} mutex_t;

int mutex_init(mutex_t *m, syn_t *s);
int mutex_lock(mutex_t *m, struct thread *thr, uint64_t ns);
int mutex_wait_cancel(mutex_t *m, struct thread *thr);
int mutex_trylock(mutex_t *m, struct thread *thr);
int mutex_unlock(mutex_t *m, struct thread *thr);

#endif /* MUTEX_H_ */