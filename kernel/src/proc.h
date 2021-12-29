#ifndef PROC_H
#define PROC_H

#include <syn/ksyn.h>
#include <common/resm.h>
#include "thread.h"
#include "mem/vm.h"
#include <common/namespace.h>

struct process {
    int pid;                    // !< Номер процесса
    struct proc_header *hdr;
    int tid;                    //!< Установленный номер первого потока
    int prio;                   //!< Базовый приоритет процесса
                                //!< первого потока, а также по умолчанию всех создаваемых потоков процесса
    int argtype;                //!< Тип входных данных процесса
    void *argv;                 //!< Входные данные для процесса, могут быть представлены как
                                //   (int argc, char *argv[]) для функции main стандартного процесса
                                //   (в этом случае arglen преобразуется в argc для передачи в main)
                                //   или применяться как массив байт с отличным
                                //   от стандартной библиотеки C кодом в точке входа
    size_t arglen;              //!< Длина входных данных, байт
    struct thread *threads;     //!< Последний поток и список потоков по thread.inproc_list
    atomic_t active_threads;    //!< Число активных потоков, участвующих в диспетчеризации (READY, RUNNING, BLOCKED)

    kobject_lock_t lock;
    int tid_alloc;              //!< Текущий номер потока для аллокатора потоков
    struct mmap *mmap;          //!< Карта памяти процесса


    struct process *parent;     //!< Родительский процесс, NULL - ядро
    struct process *children;   //!< Первый дочерний и список дочерних процессов
    struct {
        struct process *prev;   //!< Предыдущий процесс родителя parent
        struct process *next;   //!< Следующий процесс родителя parent
    } list;

    // Хранилище номеров созданных объектов синхронизации (пока только для их чистки)
    // для удобства, универсальности и красоты кода реализуем через контейнер, хоть
    // и с потерей производительности по сравнению с чистым двусвязным списком
    // управляется только модулем syn!
    res_container_t syns;

    // Хранилище номеров и ссылок на открытые объекты синхронизации, для которых получен доступ.
    // Носит формальный характер разрешений для работы, пока без объектов соединений синхронизации,
    // так как не поддерживается удаленная синхронизация.
    // Выполнение текущей работы с объектами синхронизации требует быстрой проверки доступности
    // ресурса для данного процесса, это можно сделать либо табличным доступом либо
    // с помощью к-ч дерева. Не будем ограничивать себя таблицей соединений и вводить отдельное
    // понятие id соединений с объектами синхронизации в API OS.
    // Реализуем универсальный механизм на базе контейнера ресурсов по id объектов синхронизации,
    // а не на базе простого к-ч дерева (для универсальности и возможных дополнений/изменений
    // в данной части)
    // управляется только модулем syn!
    res_container_t syns_opened;

    res_container_t *channels;
    res_container_t *connections;

//    struct {
//        int uid;
//        int gid;
//    } auth;

    int perm_time;              //!< допустимое процентное время
    int available_cores;        //!< доступные ядра (битовое поле)

    struct {
        int threads_cnt;        //!< Общее число потоков процесса
        int children_cnt;       //!< Общее число порожденных процессов
    } stat;

    struct {
        struct channel *ch;
        struct connection *parent;
        res_container_t childs;
    } signal;
};

#define IDLE_PID            (1)
extern struct process *idle_proc;

#define DYNAMIC_ALLOC_ID    (-1)

extern struct process kproc;

static inline int isKernelPID(int pid) {
//    if(pid < 10) {
//        return 1;
//    }
    switch(pid) {
    case IDLE_PID:
        return 1;
    default:
        return 0;
    }
}


/** \brief Инициализация аллокатора процессов.
     Аллокатор может быть построен на базе массива, списка или кч-дерева,
     его нужно проинициализировать.
 */
void proc_allocator_init();
void proc_allocator_lock();
void proc_allocator_unlock();

namespace_t *get_proc_namespace();

void proc_init_idle ();
void proc_init_kernel ();


/** \brief Проверка на идентичность двух процессов, то есть на то, что это один процесс.
 * Применяется для идентификации принадлежности потоков к одному процессу при выполнении
 * некоторых системных вызовов, где требуется чтобы один поток мог взаимодействовать
 * только с потоками того же процесса.
 * TODO Метод должен не только сравнивать указателии на процессы и их pid, но и
 * гарантировать сравнение с учетом жизненного цикла процессов(создания и удаления)
 * и возможным захватом pid и адреса структуры процесса разными процессами
*/
static inline int proc_equals(struct process *p1, struct process *p2) {
    if( (p1 == p2) && (p1->pid == p2->pid) && (p1->hdr == p2->hdr) ) {
        return OK;
    }
    return ERR;
}

void proc_lock(struct process *p);
void proc_unlock(struct process *p);

/** \brief  Инициализация процесса и стартового потока без постановки в диспетчер.

    Выполняется аллокатором процессов и аллокатором потоков.
    Внимание! Данная версия работает только с номерами процессов,
    так как на низком уровне работа с процессами ведется только по номерам.
    Поддержка именованных процессов ведется внешним аллокатором, связанным с VFS системы.

    \param IN/OUT pid желаемый номер для захвата,
        если pid = -1 то делаем поиск первого свободного номера
    \param OUT tid номер потока который создан
    \return enum err = OK, ERR_ILLEGAL_ARGS (например когда заголовок не правильный),
                        ERR_BUSY (процесс с таким номером запущен),
    Внимание! возможен syshalt() при нехватке памяти или свободных номеров pid
 */
int proc_init(const struct proc_header *hdr, struct proc_attr *attr, struct process *parent);

/** \brief  Получение от аллокатора процессов ссылки на объект процесса по его номеру
    \return NULL если не зарегистрирован
 */
struct process* get_proc (int pid);

int proc_free(int pid);

static inline void proc_active_threads_inc(struct process *p) {
    atomic_add(1, &p->active_threads);
}

static inline int proc_active_threads_dec(struct process *p) {
    return atomic_sub_return(1, &p->active_threads);
}

static inline int proc_active_threads(struct process *p) {
    return atomic_read(&p->active_threads);
}

/**
 *  Начальный этап завершения процесса. На время вызова включает вытеснение в системном контексте или
 *  возможны вытеснения по sched_switch при глобально запрещенных прерываниях
 *  Предназначен для выполнения длительных действий по завершению процесса:
 *  - рассылка сигналов родительскому и дочерним процессам,
 *  - закрытие объектов корневого пространства имен(каналы, объекты синхронизации и т.п.),
 *  - очистка памяти от максимально возможного числа объектов структуры процесса, которые
 *      не участвуют в завершающей стадии завершения процесса
 */
void proc_prepare_finalize(struct process *p, struct thread *last);

/**
 * Конечный этап завершения процесса. Выполняет очистку всех оставшихся ресурсов,
 * занятых процессом (включая саму структуру процесса, его карту памяти MMU).
 * Должен выполняться в безопасном контексте ядра в момент переключения на другой поток.
 */
void proc_finalize(struct process *p, struct thread *last);

#endif
