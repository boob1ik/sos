#ifndef CONFIG_H_
#ifdef __cplusplus
}
#endif
#define CONFIG_H_

#define OS_VERSION_MAJOR  "0"
#define OS_VERSION_MINOR  "0"
#define OS_VERSION_BUGFIX "0"

#define OS_VERSION        OS_VERSION_MAJOR"."OS_VERSION_MINOR"."OS_VERSION_BUGFIX

//#define NO_BOOT

#define DEBUG
#ifdef DEBUG
    #define LOGLEVEL          LOGLEVEL_ALL
#endif

//#define BUILD_SMP
#define SUPPORT_VFP

#define NUM_CORE                    ARCH_NUM_CORE       //!< максимальное число поддерживаемых ядер
#define IRQ_NUM                     161      //!< число поддерживаемых прерываний

#define PROCS_NUM                   128      //!< число поддерживаемых процессов
#define PROCS_NUM_STRLEN            3        //!< длина строки максимального номера процесса в десятичном виде
#define PROCS_PATHNAME_PREFIX       "/proc/"
#define PROCS_PATHNAME_PREFIX_LEN   6
#define PROCS_PATHNAME_SUFFIX_MAX   10    // '(signal || ...) + \0'
#define PROCS_PATHNAME_ADD_LEN      (PROCS_NUM_STRLEN + PROCS_PATHNAME_PREFIX_LEN + PROCS_PATHNAME_SUFFIX_MAX)

#define THREAD_NUM             512      //!< число поддерживаемых потоков

#define CLOCK_TICK             1000000  //!< нс
#define DEFAULT_THREAD_TICKS   1        //!< Величина кванта времени для первого потока, тиков

#define THREAD_SYSTEM_STACK_PAGES       (1)

#define TIMEOUT_MIN             1000000

#define MIN_RES_STATIC_ID       1
#define MAX_RES_STATIC_ID       (INT_MAX/2)

#define MIN_RES_DYNAMIC_ID      (MAX_RES_STATIC_ID + 1)
#define MAX_RES_DYNAMIC_ID      (INT_MAX)

#define MAX_CHANNEL_PROC        1000
#define MAX_CONNECTION_PROC     1000

#define KMEM_AUTOEXTEND_FREESIZE    (0x4000UL)  //!< свободный размер kheap для авторасширения

#define LOG_BUF_MAX             4096

#ifndef __ASSEMBLER__

typedef enum kobject_type {
    KOBJECT_SYN,                //!< ОБъект синхронизации
    KOBJECT_NUM
} kobject_type_t;

struct cfg {
    int tick;
    int ncore;
};

#endif

#ifdef __cplusplus
}
#endif
#endif /* CONFIG_H_ */
