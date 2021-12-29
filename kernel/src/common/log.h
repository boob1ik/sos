#ifndef SRC_LOG_H_
#define SRC_LOG_H_

#include <config.h>
#include <stddef.h>

#define    LOGLEVEL_ALL         0
#define    LOGLEVEL_TRACE       1
#define    LOGLEVEL_DEBUG       2
#define    LOGLEVEL_INFO        3
#define    LOGLEVEL_WARN        4
#define    LOGLEVEL_ERROR       5
#define    LOGLEVEL_FATAL       6
#define    LOGLEVEL_OFF         7

typedef enum {
    LOG_TARGET_BUFFER,
    LOG_TARGET_UART1,
    LOG_TARGET_UART2,
    LOG_TARGET_MSG_CHANNEL
} log_target_t;

void log_init (size_t single_msg_buffer);

/**
 * Функция печати журнала ядра.
 * Напрямую функцию в ядре не применять.
 *
 * Использует принятие решения о печати или пропуске сообщения на основе динамически заданного параметра уровня
 * журнала для системы и уровня журнала указанного сообщения lvl.
 * Система запускается с динамическим уровнем LOGLEVEL_ALL.
 * Макросы ниже позволяют использовать возможности статической компиляции с удалением не нужных вызовов
 * журналирования событий ядра при финальной сборке, а также с детализацией вывода сообщений
 * для одного c-файла при отладке модуля ядра, или с детализацией вывода всех сообщений ядра при отладке ядра вцелом.
 *
 * Для этого задается глобальный статический уровень журналирования в файле config.h,
 * а также может быть переопределен перед включением log.h уровень журналирования для заданного модуля ядра.
 *
 * Динамический уровень журналирования по сути является динамическим фильтром, который может быть наложен в
 * ходе работы системы. Это может применяться при отработке изделий, когда прикладной программист может
 * в любое нужное время включить и отключить из консоли более подробное журналирование ядра без его перезагрузки.
 *
 * @param lvl заданный для сообщения уровень логирования
 * @param fmt формат, стандартный для libc printf
 * @return bytes - число байт сообщения, которые удалось отправить; может быть меньше запрошенного размера сообщения
 */
size_t logmsg (int lvl, const char *fmt, ...); //!< Не применять напрямую в обычных местах, пользоваться макросами ниже
int get_loglevel ();             //!< получить
void set_loglevel (int val);     //!< установить текущий динамический уровень журналирования событий ядра

log_target_t get_log_target ();
void set_log_target (log_target_t target, ...);

#if (LOGLEVEL <= LOGLEVEL_TRACE)
#define log_trace(fmt, ...) logmsg(LOGLEVEL, fmt, ## __VA_ARGS__)
#else
#define log_trace(fmt, ...)
#endif

#if (LOGLEVEL <= LOGLEVEL_DEBUG)
#define log_debug(fmt, ...) logmsg(LOGLEVEL, fmt, ## __VA_ARGS__)
#else
#define log_debug(fmt, ...)
#endif

#if (LOGLEVEL <= LOGLEVEL_INFO)
#define log_info(fmt, ...) logmsg(LOGLEVEL, fmt, ## __VA_ARGS__)
#else
#define log_info(fmt, ...)
#endif

#if (LOGLEVEL <= LOGLEVEL_WARN)
#define log_warn(fmt, ...) logmsg(LOGLEVEL, fmt, ## __VA_ARGS__)
#else
#define log_warn(fmt, ...)
#endif

#if (LOGLEVEL <= LOGLEVEL_ERROR)
#define log_error(fmt, ...) logmsg(LOGLEVEL, fmt, ## __VA_ARGS__)
#else
#define log_error(fmt, ...)
#endif

#if (LOGLEVEL <= LOGLEVEL_FATAL)
#define log_fatal(fmt, ...) logmsg(LOGLEVEL, fmt, ## __VA_ARGS__)
#else
#define log_fatal(fmt, ...)
#endif

#endif /* SRC_LOG_H_ */
