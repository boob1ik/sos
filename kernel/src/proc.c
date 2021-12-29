#include <arch.h>
#include <common/printf.h>
#include <common/log.h>
#include "mem/kmem.h"
#include "sched.h"
#include "idle.h"
#include "common/syshalt.h"
#include "common/resm.h"
#include <rbtree.h>
#include <limits.h>
#include <syn/syn.h>
#include <syn/signal.h>
#include <ipc/channel.h>
#include <ipc/connection.h>
#include <common/namespace.h>

static struct process *proc_tbl[PROCS_NUM + 1];
static int pid_cnt;
static int pid_alloc;
static kobject_lock_t pid_allocator_lock;

static namespace_t proc_namespace;

//static struct rbtree proc_hdr_by_entry
static const char prefix[] = PROCS_PATHNAME_PREFIX;

static struct {
    struct process *first;
    struct process *last;
} root_procs;
static kobject_lock_t root_procs_lock;

static char idle_pname[48] = "idle";
static char kernel_pname[48] = "kernel";

const struct proc_header idle_hdr = {
    .magic = PROC_HEADER_MAGIC, .type = 0, .pathname = (char *)idle_pname,
    .entry = idle, .stack_size = PAGE_SIZE, .proc_seg_cnt = 0, .segs = { } };

struct process *idle_proc;

const struct proc_header kernel_hdr = {
    .magic = PROC_HEADER_MAGIC, .type = 0, .pathname = (char *)kernel_pname,
    .entry = NULL, .stack_size = PAGE_SIZE, .proc_seg_cnt = 0, .segs = { }
};

const mem_attributes_t proc_arg_pages_attr = {
    .type = MEM_TYPE_NORMAL,
    .exec = MEM_EXEC_NEVER,
    .os_access = MEM_ACCESS_RW,
    .process_access = MEM_ACCESS_RW,
    .inner_cached = MEM_CACHED_WRITE_BACK,
    .outer_cached = MEM_CACHED_WRITE_BACK,
    .shared = MEM_SHARED_OFF,
};

//! Процесс не содержит потоков, не диспетчеризируется
struct process kproc;


void *malloc(size_t size) {
    return NULL;
}
void *calloc(size_t count, size_t size) {
    return NULL;
}

void proc_allocator_init ()
{
    for (int i = 0; i < PROCS_NUM; i++)
        proc_tbl[i] = NULL;
    pid_cnt = 0;
    pid_alloc = 1;
    root_procs.first = NULL;
    root_procs.last = NULL;
    kobject_lock_init(&pid_allocator_lock);
    ns_init(&proc_namespace);
}

inline void proc_allocator_lock ()
{
    kobject_lock(&pid_allocator_lock);
}

inline void proc_allocator_unlock ()
{
    kobject_unlock(&pid_allocator_lock);
}

namespace_t *get_proc_namespace() {
    return &proc_namespace;
}

inline void proc_lock (struct process *p)
{
    kobject_lock(&p->lock);
}

inline void proc_unlock (struct process *p)
{
    kobject_unlock(&p->lock);
}

void proc_init_idle ()
{
    struct proc_attr idle_attr = {
        .pid = IDLE_PID,
        .tid = 0,
        .prio = PRIO_NUM,
        .argtype = PROC_ARGTYPE_BYTE_ARRAY,
        .argv = NULL,
        .arglen = 0
    };
    struct thread_attr idlethr_attr = {
        .entry = idle_hdr.entry,
        .arg = NULL,
        .tid = 0,
        .stack_size = idle_hdr.stack_size,
        .ts = DEFAULT_THREAD_TICKS,
        .flags = 0
    };
    struct thread *thr;
    // здесь контекст не вытесняемый, но можно применять proc_init
    proc_init(&idle_hdr, &idle_attr, &kproc);
    idle_proc = proc_tbl[IDLE_PID];
#ifndef BUILD_SMP
    // для каждого ядра создаем свой idle поток
    for (int i = 0; i < NUM_CORE - 1; i++) {
        thread_init(&idlethr_attr, idle_proc, &thr);
    }
#endif
}

void proc_init_kernel ()
{
    kproc.channels = kmalloc(sizeof(*kproc.channels));
    kproc.connections = kmalloc(sizeof(*kproc.connections));
    resm_container_init(kproc.channels, MAX_CHANNEL_PROC, RES_CONTAINER_MEM_LIMIT_DEFAULT, RES_ID_GEN_STRATEGY_INC_AGING);
    resm_container_init(kproc.connections, MAX_CONNECTION_PROC, RES_CONTAINER_MEM_LIMIT_DEFAULT, RES_ID_GEN_STRATEGY_INC_AGING);
}

/**
 * Входные:
 *  hdr - должен являться копией в карте памяти ядра, в структуру процесса переходит как ссылка
 *  attr - временный аргумент параметров, в структуру процесса переходит копированием
 *          (для attr.argv делается копия в карте памяти ядра)
 *
 *  hdr.pathname должен указывать на массив байт строки pathname с резервными байтами для добавления префикса
 *  и суффикса пути и генерации таким образом уникального объекта в rootns вида /proc/$pathname$/$pid$,
 *  для исключения необходимости дополнительной работы с динамической памятью
 * Выходные:
 * attr.pid, attr.tid, attr.prio
 */
int proc_init (const struct proc_header *hdr, struct proc_attr *attr, struct process *parent)
{
    struct thread_attr tattrs;
    struct thread *thr;
    struct mmap *cmap = (parent == NULL) ? NULL : parent->mmap;
    struct seg *pseg;
    int tmp;
    int newpid;

    // быстрые проверки
    if ((hdr == NULL) || (hdr->magic != PROC_HEADER_MAGIC) || (attr == NULL)) {
        return ERR_ILLEGAL_ARGS;
    }
    // проверки корректности заголовка процесса
    // TODO
    // проверки корректности атрибутов запуска
    // TODO

    if(attr->arglen > mmu_page_size()) {
        return ERR_ILLEGAL_ARGS;
    }

    newpid = attr->pid;
    if (newpid < 0) {
        newpid = pid_alloc;
    } else if(newpid >= PROCS_NUM) {
        return ERR_ILLEGAL_ARGS;
    } else if(proc_tbl[newpid] != NULL) {
        return ERR_BUSY;
    }
    struct process *p = (struct process *) kmalloc(sizeof(struct process));
    memset(p, 0, sizeof(struct process));

    tmp = newpid;
    // захват номера для нового процесса
    proc_allocator_lock();
    if (pid_cnt > PROCS_NUM) {
        proc_allocator_unlock();
        kfree(p);
        return ERR_PROC_LIMIT;
    }
    // проверку прошли, должен быть хотя бы один свободный номер в таблице
    // TODO линейный аллокатор pid нужно переделать на resmgr
    while (1) {
        if (proc_tbl[newpid] == NULL) {
            proc_tbl[newpid] = p;
            p->pid = newpid;
            pid_cnt++;
            pid_alloc++;
            if (pid_alloc >= PROCS_NUM) {
                pid_alloc = 1;
            }
            break;
        } else {
            newpid++;
            if (newpid >= PROCS_NUM)
                newpid = 1;
            if (newpid == tmp) {
                // прошли по кругу, не найден свободный pid, не может быть
                proc_allocator_unlock();
                syshalt(SYSHALT_PID_LIMIT);
            }
        }
    }
    proc_allocator_unlock();

    p->hdr = (struct proc_header *) hdr;
    p->prio = attr->prio;
    if (isKernelPID(p->pid)) {
        p->mmap = kmap;
    } else {
        p->mmap = vm_map_create();
    }
    for (int i = 0; i < hdr->proc_seg_cnt; i++) {
        if ((hdr->segs[i].size >> PAGE_SIZE_SHIFT) == 0) {
            continue;
        }

        if ((cmap != NULL)
                && ((pseg = vm_seg_get(cmap, hdr->segs[i].adr)) != NULL)) {
            if(pseg->attr.multu_alloc) {
                tmp = vm_map(p, hdr->segs[i].adr,
                        hdr->segs[i].size >> PAGE_SIZE_SHIFT, hdr->segs[i].attr);
            } else {
                tmp = vm_seg_move(cmap, p->mmap, pseg);
            }
        } else {
            tmp = vm_map(p, hdr->segs[i].adr,
                    hdr->segs[i].size >> PAGE_SIZE_SHIFT, hdr->segs[i].attr);
        }
        if (tmp != OK) {
            while(i > 0) {
                i--;
                vm_free(p, hdr->segs[i].adr);
            }
            if(p->mmap != kmap) {
                vm_map_terminate(p->mmap);
            }
            kfree(p);
            return tmp;
        }
    }

    // теперь инициализация, ее вроде можно выполнить вне защищенного кода выше,
    // так как пока процесс не участвует в работе (ресурсы не захвачены и поток не запущен)
    // TODO проверить, возможно или нет и не приведет ли к плохому обращение к структуре
    // по get_proc(pid), оно может уже работать после разблокировки аллокатора
    // и возвращать ссылку на неполностью готовую структуру процесса

    // Сформируем уникальное полное имя и путь процесса для rootns,
    // для чего переместим pathname в /proc и создадим таким образом ссылку в rootns на процесс /proc/$pathname$/$pid$
    int pathname_length = strlen(p->hdr->pathname);
    char pid_str[PROCS_NUM_STRLEN + 2];
    if(pathname_length > 0) {
        snprintf(pid_str, PROCS_NUM_STRLEN + 2, "/%i", p->pid);
    } else {
        snprintf(pid_str, PROCS_NUM_STRLEN + 2, "%i", p->pid);
    }
    char *src = p->hdr->pathname + pathname_length, *dst = src + PROCS_PATHNAME_PREFIX_LEN;
    strcpy(dst, pid_str);
    for(int i = pathname_length; i > 0; i--) {
        dst--;
        src--;
        *dst = *src;
    }
    for(int i = PROCS_PATHNAME_PREFIX_LEN - 1; i >= 0; i--) {
        dst--;
        *dst = prefix[i];
    }

    if ( (p->pid != IDLE_PID) &&
            ((p->prio > PRIO_MIN) || (p->prio < PRIO_MAX)) ) {
        if(parent != NULL) {
            p->prio = parent->prio;
        } else {
            p->prio = PRIO_DEFAULT;
        }
    }

    // аргументы argv
    p->argtype = attr->argtype;
    if( (attr->argv != NULL) && (attr->arglen > 0) ) {
        p->arglen = attr->arglen;
        p->argv = vm_alloc(parent, 1, proc_arg_pages_attr);
        memcpy(p->argv, attr->argv, p->arglen);
        if(p->argtype == PROC_ARGTYPE_STRING_ARRAY) {
            // массив указателей скопирован, теперь нужно скопировать сами строки в ту же страницу
            // и модифицировать указатели на строки в новом месте, безопасность операции проверяется в системном вызове
            char *dst = p->argv + p->arglen;
            char **tbl = p->argv;
            char *src;
            while((src = *tbl) != NULL) {
                *tbl = dst; // переключаем указатель
                tbl++;
                do {
                    *dst++ = *src++;
                } while(*src != '\0'); // копируем строку вручную без проверок уже
                *dst++ = '\0';
            }
        }
        vm_seg_move (parent->mmap, p->mmap, vm_seg_get(parent->mmap, p->argv));
    } else {
        p->arglen = 0;
        p->argv = NULL;
    }

    kobject_lock_init(&p->lock);
    p->tid_alloc = p->pid;
    p->children = NULL;
    p->parent = parent;
    p->threads = NULL;

    // хранилища по синхронизации инициализируем статическими по захвату всех номеров

    resm_container_init (&p->syns, RES_CONTAINER_NUM_LIMIT_DEFAULT,
            RES_CONTAINER_MEM_LIMIT_DEFAULT, RES_ID_GEN_STRATEGY_NOGEN);
    resm_container_init (&p->syns_opened, RES_CONTAINER_NUM_LIMIT_DEFAULT,
            RES_CONTAINER_MEM_LIMIT_DEFAULT, RES_ID_GEN_STRATEGY_NOGEN);

    log_info("+pid %i '%s'; prio=%i, entry=0x%08lx\n\r", p->pid, hdr->pathname, p->prio, hdr->entry);

    // пока считаем что либо создается поток либо syshalt
    // аргументы первого потока берутся из процесса
    tattrs.arg = NULL;
    tattrs.entry = hdr->entry;
    tattrs.flags = 0;
    tattrs.stack_size = hdr->stack_size;
    tattrs.ts = DEFAULT_THREAD_TICKS;
    tattrs.type = THREAD_TYPE_JOINABLE;
    thread_init(&tattrs, p, &thr);

    p->list.prev = NULL;
    p->list.next = NULL;
    // Правим список дочерних процессов у родительского уже после всего,
    // когда процесс инициализирован
    if (p->parent != NULL) {
        proc_lock(p->parent);
        p->parent->list.next = p;
        p->list.prev = p->parent->list.prev;
        proc_unlock(p->parent);
    } else {
        kobject_lock(&root_procs_lock);
        if (root_procs.first == NULL) {
            root_procs.first = p;
            root_procs.last = p;
        } else {
            root_procs.last->list.next = p;
            p->list.prev = root_procs.last;
            root_procs.last = p;
        }
        kobject_unlock(&root_procs_lock);
    }

    p->channels = kmalloc(sizeof(*p->channels));
    resm_container_init(p->channels, MAX_CHANNEL_PROC, RES_CONTAINER_MEM_LIMIT_DEFAULT, RES_ID_GEN_STRATEGY_INC_AGING);
    p->connections = kmalloc(sizeof(*p->connections));
    resm_container_init(p->connections, MAX_CONNECTION_PROC, RES_CONTAINER_MEM_LIMIT_DEFAULT, RES_ID_GEN_STRATEGY_INC_AGING);

    attr->pid = p->pid;
    attr->prio = p->prio;
    attr->tid = thr->tid;
    if (p->pid != IDLE_PID) {
        thread_run(thr);
    }
    return OK;
}

struct process* get_proc (int pid) {
    if ((pid >= PROCS_NUM) || (pid < 1)) {
        return NULL;
    }
    return proc_tbl[pid];
}

static void send_signals_on_finalize (struct process *p, struct thread *last_thread) {
    // TODO
    struct process *child;
//    proc_lock(p);
    union sigval val = {.value_int = 0};
    if(p->parent != NULL) {
        signal_to_proc(last_thread, p->parent, SIGCHLD, p->pid, val); // TODO value - код завершения последнего потока
    }
    child = p->children;
    while(child != NULL) {
        signal_to_proc(last_thread, child, SIGHUP, p->pid, val);
        child = child->list.next;
    }
//    proc_unlock(p);
}

void proc_prepare_finalize(struct process *p, struct thread *last_thread)
{
    last_thread->substate = PROC_FINALIZE;     // NORMAL   -> PROC_FINALIZING
    send_signals_on_finalize(p, last_thread);

    close_channels(p);
    close_connections(p);

    kfree(p->channels);
    p->channels = NULL;
    kfree(p->connections);
    p->connections = NULL;

    syn_proc_finalize(p);
}


void proc_finalize (struct process *p, struct thread *last_thread)
{
    struct thread *thr = p->threads;
    while(thr != NULL) {
        if(thr == last_thread) {
            thr = thr->inproc_list.next;
            continue;
        }
        if(thr->irqctx != NULL) {
            // так как метод выполняется синхронно между вызовами обработчиков прерываний, то достаточно выполнить
            interrupt_release(thr->irqctx->id);
        }
        // рассматриваемые потоки находятся в STOPPED состоянии (в силу работы алгоритма счетчика активных потоков
        // и вызова proc_finalize только когда он равен 0), поэтому освобождаем номера tid и память занятую ими в kmap
        thread_finalize (thr);
        thr = thr->inproc_list.next;
    }

    thread_finalize(last_thread);   // теперь последний поток можно удалить тоже
    proc_allocator_lock();
    log_info("-pid %i '%s'\n\r", p->pid, p->hdr->pathname);
    proc_tbl[p->pid] = NULL;
    pid_cnt--;
    proc_allocator_unlock();

    vm_map_terminate(p->mmap);
    kfree(p->hdr);
    p->hdr = NULL;
    kfree(p);
}
