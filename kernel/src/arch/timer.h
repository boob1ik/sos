#ifndef ARCH_TIMER_H_
#define ARCH_TIMER_H_
#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>

/**
 * Модуль таймера ОС должен обеспечивать:
 * 1) периодические прерывания (настройка прерываний выполняется внешним кодом)
 *      с заданным периодом или близким к нему по мере возможностей аппаратного таймера(ов),
 *      предполагая что метод timer_event() будет вызываться по каждому событию внешним кодом
 *      для обратной связи с модулем таймера.
 * 2) установку прерывания по одному событию, которое наступит раньше периодического события,
 *    в этом случае при возникновении пользовательского события отсчет нового периода
 *    периодических прерываний должен начаться сначала по умолчанию
 * 3) подсчет системного времени в нс в методе timer_event() и дополнительно по требованию
 *      в методе time_update(), должно обеспечиваться отсутствие набегающей ошибки подсчета
 *      (соответствие реальному времени задающей частоты таймеров)
 * 4) Для SMP режима предполагается централизованная политика подсчета времени
 *      и диспетчеризации, при которой CPU_0 принимается за главный процессор, который
 *      обрабатывает таймерные прерывания от глобального таймера.
 *      Вторичные процессоры получают текущее время через глобальные статические переменные
 *      и текущие значения глобального таймера. При этом только ядро CPU_0 является ответсвенным
 *      за возможную корректировку времени срабатывания таймерного прерывания по timer_set_event.
 *
 * Модуль может быть построен на одном или нескольких аппаратных таймерах(не оговаривается)
 */

/**
 * Инициализация модуля с плановым периодом обновления системного времени и
 * активации диспетчера задач, период в нс
 */
int timer_init(uint64_t period_ns);

/**
 * Получение периода, на который настроен таймер.
 * Он может отличаться от затребованного значения при аппаратных ограничениях таймера.
 * Возвращаемое значение в нс может быть приблизительным (не точным), если не является
 * кратным нс (например, таймер работает от частоты 32768 Hz)
 */
uint64_t timer_get_period();

/**
 * Включение таймера
 */
int timer_enable();

/**
 * Отключение таймера
 */
int timer_disable();

/**
 * Установка события таймерного времени пользователем (ядром ОС), которое
 * наступит раньше запланированного периодического или планового прерывания.
 * Если заданное событие наступит позже текущего, то
 * метод не должен менять работу таймера и запланированный момент прерывания.
 * Метод должен обеспечивать коррекцию только в случае, если текущее время (значение счетчика)
 * и текущий установленный момент прерывания позволяют установить более раннее срабатывание
 * прерывания.
 * Предполагается, что в SMP режиме метод работает только для CPU_0
 * в защищенном от прерывания коде.
 */
int timer_set_event(uint64_t event_time_ns);

/**
 * Обратная связь с внешним кодом, обработчик события таймерного прерывания для модуля.
 * Должен обеспечивать обновление системного времени.
 * Предполагается, что метод вызывается всегда в защищенном коде (обработчике прерывания без
 * разрешенных вложенных прерываний)
 */
void timer_event();

/**
 * Обновление значения системного времени для обеспечения подсчета в условиях
 * аппаратных ограничений таймера.
 * Метод должен выполняться в защищенной секции кода.
 * В SMP режиме должен позволять обновлять время только ядру CPU_0
 */
void time_update();

/**
 * Получение значения системного времени от начала старта,
 * основной метод для применения внутри ядра
 */
uint64_t systime();

/**
 * Получение значения реального OS_CLOCK_REALTIME или системного OS_CLOCK_MONOTONIC времени в нс.
 * Метод может выполняться с разрешенными прерываниями или на разных ядрах в SMP режиме и,
 * соответственно, должен гарантировать целостность значения времени.
 */
int time_get(int clock_id, kernel_time_t *tv);

/**
 * Установка реального времени системы, не должна приводить к изменению хода
 * системного времени от старта, также должна обеспечиваться целостность значения времени.
 * Предполагается, что метод будет вызываться только одним драйвером реального времени
 * (одним потоком), поэтому реализация обеспечения целостности может сводиться к
 * переключению номера активного значения реального времени после корректировки
 * неиспользуемого значения в данный момент (изменение номера может быть выполнено атомарно)
 *
 * Метод должен возвращать ошибку при попытке установки не нормализованного значения
 * OS_CLOCK_REALTIME в котором tv_nsec превосходит время в 1 сек
 */
int time_set(int clock_id, kernel_time_t *tv);

#ifdef __cplusplus
}
#endif
#endif /* TIMER_H_ */
