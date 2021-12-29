#ifndef OS_TYPES_H
#define OS_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define ARG_MAX             DEFAULT_PAGE_SIZE
#define PATH_MAX            255
#include <limits.h>
#include <reent.h>

#define __syscall   static inline __attribute__ ((__always_inline__)) __attribute__ ((optimize(0)))

typedef enum err {
    OK = 0,

    ERR = -255,
    ERR_ILLEGAL_ARGS,
    ERR_PATHNAME_TOO_LONG,
    ERR_ACCESS_DENIED,
    ERR_NO_MEM,
    ERR_BUSY,
    ERR_TIMEOUT,
    ERR_THREAD_LIMIT,
    ERR_PROC_LIMIT,
    ERR_DEAD,

    ERR_NOT_USED = -255,
    ERR_PROC_,
    ERR_THREAD_,

    ERR_IPC_PATHNAME,
    ERR_IPC_NA,
    ERR_IPC_ILLEGAL_CHANNEL,
    ERR_IPC_ILLEGAL_COMMAND,
    ERR_IPC_ILLEGAL_PRIVATE,
    ERR_IPC_REPLY_CONNECTION,
    ERR_IPC_REPLY_CHANNEL,
    ERR_IPC_SHARE,
    ERR_IPC_REDIRECT,
    ERR_IPC_CHANNEL_CLOSED,
    ERR_IPC_CHANNEL_NOTFOUND,
    ERR_IPC_CONNECTION_CLOSED,

    ERR_MEMORY_,
    ERR_IRQ_,
    ERR_SYN_,
    ERR_SHUTDOWN_,
    ERR_INFO_,
    ERR_TIME_,
    ERR_CTRL_,
} err_t;           //!< результаты выполнения системных вызовов

static inline err_t syscall_error (int res) { return (res >= 0) ? 0 : res; }

#define SYSCALL_ERROR(sc)                   (sc < 0)
#define SYSCALL_ERROR_SET(sc, result)       ((result = sc) < 0)
#define SYSCALL_SUCCESS(sc)                 (sc >= 0)
#define SYSCALL_SUCCESS_OK(sc)              (sc == 0)
#define SYSCALL_SUCCESS_SET(sc, result)     ((result = sc) >= 0)

typedef int pid_t; //!< Тип идентификатора номера процесса
typedef int tid_t; //!< Тип идентификатора номера потока в процессе

typedef enum mem_shared {
    MEM_SHARED_OFF, //!< для одноядерных систем, а также память для работы на одном ядре процессора в SMP режиме
    MEM_SHARED_ON   //!< только для многоядерных систем, поддержка режима SMP с обеспечением целостности операций чт/зп
} mem_shared_t;

typedef enum mem_type {
    MEM_TYPE_STRONGLY_ORDERED,
    MEM_TYPE_DEVICE,
    MEM_TYPE_NORMAL
} mem_type_t;

typedef enum mem_execution {
    MEM_EXEC_ON,                    //!< исполняемый код
    MEM_EXEC_NEVER,                 //!< только данные
} mem_execution_t;

typedef enum mem_cache_type {
    MEM_CACHED_OFF,
    MEM_CACHED_WRITE_BACK_WRITE_ALLOCATE,
    MEM_CACHED_WRITE_THROUGH,
    MEM_CACHED_WRITE_BACK
} mem_cache_type_t;

typedef enum mem_access {
    MEM_ACCESS_NO,                  //!< отсутствует или доступ запрещен
    MEM_ACCESS_RO,                  //!< доступ только на чтение
    MEM_ACCESS_RW,                  //!< доступ на чтение и запись
} mem_access_t;

typedef enum mem_security {
    MEM_SECURITY_OFF,
    MEM_SECURITY_ON
} mem_security_t;

typedef enum mem_multi_alloc {
    MEM_MULTU_ALLOC_OFF,
    MEM_MULTU_ALLOC_ON              //!< разрешение захвата одного сегмента страничной памяти несколькими процессами
} mem_multi_alloc_t;

typedef struct mem_attributes {
        enum mem_shared shared              :1;
        enum mem_execution exec             :1;
        enum mem_type type                  :2;
        enum mem_cache_type inner_cached    :2;
        enum mem_cache_type outer_cached    :2;
        enum mem_access process_access      :2;
        enum mem_access os_access           :2;
        enum mem_security security          :1;
        enum mem_multi_alloc multu_alloc    :1;
        unsigned long                       :18;
} mem_attributes_t;

typedef struct proc_seg {
    void *adr;
    size_t size;
    mem_attributes_t attr;
} proc_seg_t;

#define PROC_HEADER_MAGIC       (0x434f5250u) // ASCII 'PROC'
#define PATHNAME_MAX_LENGTH     PATH_MAX
#define PATHNAME_MAX_BUF        PATHNAME_MAX_LENGTH + 1
typedef struct proc_header {
    uint32_t magic;                 //!< Идентификатор заголовка процесса \PROC_HEADER_MAGIC
    uint32_t type;                  //!< Тип процесса  пока не исп TODO
    char *pathname;                 //!< Имя процесса с путем в корневом пространстве имен
    void *entry;                    //!< Точка входа кода инициализации и запуска первого потока
    uint32_t stack_size;            //!< Стартовый размер стека первого потока
    int proc_seg_cnt;               //!< Число сегментов памяти процесса для регистрации в системе
    struct proc_seg segs[];         //!< Таблица параметров сегментов (адреса, размеры, атрибуты)
} proc_header_t;

typedef struct proc_attr {
    int pid;                        //!< param[out]    Установленный номер процесса
    int tid;                        //!< param[out]    Установленный номер первого потока
    int prio;                       //!< param[in/out] Приоритет
    int argtype;                    //!< param[in]     Тип входных данных для процесса
    void *argv;                     //!< param[in]     Входные данные для процесса
    size_t arglen;                  //!< param[in]     Длина входных данных, байт
} proc_attr_t;

#define PROC_ARGTYPE_BYTE_ARRAY      0
#define PROC_ARGTYPE_STRING_ARRAY    1

typedef struct proc_info {
    pid_t parent;                   //!< param[out]
    uint32_t nthreads;              //!< param[out]
    size_t mem_allocated;           //!< param[out]
    size_t mem_mapped;              //!< param[out]
    char name[PATHNAME_MAX_BUF];    //!< param[out]
} proc_info_t;

typedef enum thread_type {
    THREAD_TYPE_JOINABLE,
    THREAD_TYPE_DETACHED,
    THREAD_TYPE_IRQ_HANDLER
} thread_type_t;

typedef struct thread_attr {
    void *entry;                    //!< param[in]      Стартовая функция потока
    void *arg;                      //!< param[in]      Аргумент стартовой функции
    int tid;                        //!< param[out]     Установленный номер потока
    int stack_size;                 //!< param[in]      Размер стека потока
    int ts;                         //!< param[in]      Размер кванта (в тиках)
    int flags;                      //!< param[in]      Флаговые параметры
    int type;                       //!< param[in]      Тип потока
} thread_attr_t;

#define MAX_THREAD_TICKS        10       //!< Максимально допустимое число тиков потока

enum prio {
    PRIO_GET_CURRENT = -1,
    PRIO_MAX = 8,
    PRIO_DEFAULT = 32,
    PRIO_MIN = 63,
    PRIO_NUM
};

#define PRIORITY_IS_VALID(p)        ( (p <= PRIO_MIN) && (p >= PRIO_MAX) )
#define PRIORITY_IS_INVALID(p)      ( (p > PRIO_MIN) && (p < PRIO_MAX) )

enum irq_assert_type {
    IRQ_ASSERT_DEFAULT,
    IRQ_ASSERT_LEVEL_LOW,
    IRQ_ASSERT_LEVEL_HIGH,
    IRQ_ASSERT_EDGE_RISING,
    IRQ_ASSERT_EDGE_FALLING,
};

typedef struct irq_ctrl {
        uint32_t assert_type        :8;
        uint32_t enable             :1;
        uint32_t                    :23;
} irq_ctrl_t;

/**
    Канал сигналов создается при создании процесса. Доступ к ID из библиотеки.

    Соединения к сигналам процесса устанавливаются явно при прикладного кода.

    Системные сигналы доставляются из кода ядра (соединения устанавливаются автоматически).
*/
typedef enum signal {
    SIGHUP  = 1,    //!< Сигнал дочерним процессам о завершении процесса
    SIGINT  = 2,    //!< Прерывание, сигнал оповещения о событии
    SIGQUIT = 3, //!< SIGQUIT
    SIGILL  = 4,    //!< Ошибка MMU выполнения команд из сегмента памяти, который не помечен как исполняемый
    SIGTRAP = 5, //!< SIGTRAP
    SIGIOT  = 6, //!< SIGIOT
    SIGABRT = 6,    //!< Завершение процесса по критическому событию по собственной инициативе функцией LIBC abort()
    SIGEMT  = 7, //!< SIGEMT
    SIGFPE  = 8,    //!< Ошибка арифметических операций, например переполнение или деление на ноль.
    SIGKILL = 9,    //!< Оповещение о принудительном закрытии процесса
    SIGBUS  = 10,   //!< Ошибка на шине
    SIGSEGV = 11,   //!< Ошибка MMU доступа к памяти, память не зарегистрирована или отказ доступа по атрибутам
    SIGSYS  = 12,   //!< Ошибка в аргументе системного вызова
    SIGPIPE = 13,   //!<
    SIGALRM = 14,   //!< Срабатывание таймера
    SIGTERM = 15,   //!< Запрос процессу на его завершение

    SIGURG  = 16,//!< SIGURG
    SIGSTOP = 17,//!< SIGSTOP
    SIGTSTP = 18,//!< SIGTSTP
    SIGCONT = 19,//!< SIGCONT
    SIGCHLD = 20,   //!< Сигнал родительскому процессу о завершении процесса
    SIGCLD  = 20,//!< SIGCLD
    NSIG = 32    //!< NSIG
} signal_t;

/** \brief Информация о памяти. */
struct mem_info {
    size_t page_size;
    struct {
        size_t size;
        size_t free;
    } heap;
    struct {
        size_t size;
        size_t segs;
    } map;
    struct {
        size_t size;
        size_t free;
    } allocated;
    struct {
        size_t size;
        size_t free;
    } mapped;
    int maps;
    int segs;
    int pgts;
};

union os_info {
//struct kernel_info  kernel;
    struct mem_info mem;
//struct irq_info irq;
//struct debug_info debug;
};

/**
 * Область системных статических переменных потока, к которым всегда открыт быстрый доступ.
 * Каждый поток имеет свою структуру thread_tls_user_t.
 */
typedef struct thread_tls_user {
    int tid;                    //! < Номер потока
    int pid;                    //! < Номер процесса
    struct _reent reent;        //! < Контекст исполнения стандартной библиотеки libc, отдельный для каждого потока
} thread_tls_user_t;

typedef struct os_ctrl {
    uint32_t tick;
    uint32_t sched_algorithm;
} os_ctrl_t;

#define OS_CLOCK_MONOTONIC     1
#define OS_CLOCK_REALTIME      2

#define TIMEOUT_1_MS            1000000ULL
#define TIMEOUT_10_MS           10000000ULL
#define TIMEOUT_100_MS          100000000ULL
#define TIMEOUT_1_SEC           1000000000ULL
#define TIMEOUT_INFINITY        UINT64_MAX
#define NO_WAIT                 0

#define DYNAMIC_ID              0
#define NO_FLAGS                0
#define DEFAULT_STACK           0

// На уровне микроядра ОС значение времени OS_CLOCK_MONOTONIC от старта системы
// представляем типом uint64_t в нс.
// Все события в микроядре ОС (таймауты и диспетчеризация) обрабатываются по значениям
// времени OS_CLOCK_MONOTONIC.
// Для поддержки стандарта POSIX по работе с реальным временем OS_CLOCK_REALTIME, учитывая
// проблему 2038 для time_t в 32 бита в struct timespec, для ОС используем расширенную структуру
// kernel_time_t. При получении от ОС времени OS_CLOCK_MONOTONIC значение tv_sec = 0
typedef struct kernel_time {
    uint64_t tv_sec;
    uint64_t tv_nsec;
} kernel_time_t;

typedef struct atomic32 {
    int32_t __attribute__((aligned(4))) val;
} atomic32_t;

typedef struct atomic64 {
    int64_t __attribute__((aligned(8))) val;
} atomic64_t;

typedef atomic32_t atomic_t;

typedef struct spinlock {
    volatile unsigned int lock;
} spinlock_t;

typedef enum syn_type {
    MUTEX_TYPE_PLOCAL,     //!< быстрый мьютекс, доступен потокам одного процесса, размещается в памяти процесса
    MUTEX_TYPE_PSHARED,    //!< мьютекс, доступен другим процессам, размещается в микроядре ОС
    SEMAPHORE_TYPE_PLOCAL, //!< быстрый семафор, доступен потокам одного процесса, размещается в памяти процесса
    SEMAPHORE_TYPE_PSHARED,//!< семафор, доступен другим процессам, размещается в микроядре ОС
    BARRIER_TYPE_PLOCAL,   //!< барьер, доступен потокам одного процесса, размещается в микроядре ОС
    BARRIER_TYPE_PSHARED   //!< барьер, доступен другим процессам, размещается в микроядре ОС
} syn_type_t;

/**
 * Быстрые мьютексы и семафоры доступны по умолчанию только внутри процесса.
 * В то же время путем разрешения доступа к странице памяти, где инициализирован объект
 * синхронизации, возможна быстрая межпроцессная синхронизация. Для этого необходимо выделять
 * отдельную страницу(ы) памяти через системный вызов. Сам процесс разрешения доступа выполняется
 * передачей системного сообщения целевым процессам в доступные каналы сообщений, что не всегда
 * возможно и приемлемо.
 */

/**
 * Структура инициализации объекта синхронизации,
 * для объектов типа MUTEX_TYPE_PLOCAL и SEMAPHORE_TYPE_PLOCAL она также является
 * рабочим объектом синхронизации в памяти, доступной процессу
 */
typedef struct syn {
    int id;                     //! [out] идентификатор для работы в процессе-создателе
    enum syn_type   type;       //! [in]  тип объекта синхронизации
    char            *pathname;  //! [in]  имя и путь в VFS, кроме MUTEX_TYPE_PLOCAL
    atomic_t        cnt;        //! рабочий счетчик MUTEX_TYPE_PLOCAL и SEMAPHORE_TYPE_PLOCAL
    union {
        volatile int owner_tid; //! номер потока-захватчика, только для MUTEX_TYPE_PLOCAL
                                // модифицируется без защиты от вытеснения
                                // потоком после быстрого захвата (без системного
                                // вызова), применяется для проверки при
                                // быстром снятии блокировки, защита не предусматривается
                                // ввиду особого применения этого поля самим потоком для
                                // самоконтроля, однако после выполнения системных вызовов
                                // syn_wait и syn_done поле также модифицируется ядром
        int limit;              //! [in] для семафоров и барьеров - максимальное значение счетчика
    };
} syn_t;

typedef enum sys_msg_type {
    SYS_MSG_MEM_SEND,                           //!< передача во владение страничной памяти другому процессу
    SYS_MSG_MEM_SHARE,                          //!< разрешение доступа к страничной памяти другому процессу
    SYS_MSG_SIGNAL,                             //!< передача сигналов
} sys_msg_type_t;

typedef struct sys_msg_mem {
    enum sys_msg_type _type;                    //!< SYS_MSG_MEM_SEND или SYS_MSG_MEM_SHARE
    struct proc_seg seg;                        //!< информация о сегменте страничной памяти
} sys_msg_mem_t;

union sigval {
    int value_int;
    void *value_ptr;
};

typedef struct sys_msg_signal_event {
    enum sys_msg_type _type;                    //!< SYS_MSG_SIGNAL
    int pid;                                    //!< процесс-источник события, 0 - микроядро
    int tid;                                    //!< поток-источник события, 0 - событие из другого процесса
    int num;                                    //!< номер сигнала
    int code;                                   //!< дополнительнай код
    union sigval value;                         //!< данные
} sys_msg_signal_event_t;        //!< операция передачи сигналов


/** формат сообщения */
typedef struct msg {
    union {
        void *ptr;
        sys_msg_mem_t  *memcmd;                 //!< операция обмена страничной памятью между процессами
        sys_msg_signal_event_t *sigevent;       //!< операция передачи сигналов
    } sys;
    size_t size;              //!< размер данных пользователя, байт
    void *data;               //!< данные пользователя
} msg_t;

/** Локальная папка процесса.
    Система обеспечивает уникальность полного имени канала открытого в этой папке
    по отношению к другим процессам.
*/
#define PROCESS_HOME           "$"
#define SIGNAL_CHANNEL         PROCESS_HOME"/signal"   //!< канала доставки сообщений процесса

/** Cистемная папка ядра
    Каналы открытые в этой папке реализуют вынесенные функции ядра (используются в системных вызовах).
*/
#define KERNEL_HOME            "#"

/** Список подддерживаемых каналов ядра */
#define LOGGER_CHANNEL         KERNEL_HOME"/logger"  //!< канал журналирования событий микроядра
#define SECURE_CHANNEL         KERNEL_HOME"/secure"  //!< канал менеджера безопасности

typedef enum channel_type {
    CHANNEL_PUBLIC,
    CHANNEL_PROTECTED,
    CHANNEL_PRIVATE,
} channel_type_t;

struct pathname {
    char absolute[PATHNAME_MAX_BUF];
    char *suffix;
};

struct connection_info {
    int pid;
    int conid; // в пространстве pid'a
    struct pathname pathname;
};

enum connection_cmd {
    CONNECTION_DENY,
    CONNECTION_ALLOW,
    CONNECTION_TO_PRIVATE,
    CONNECTION_CONTINUE,
};

#ifdef __cplusplus
}
#endif
#endif
