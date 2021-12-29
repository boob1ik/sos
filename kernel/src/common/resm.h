#ifndef RESM_H_
#define RESM_H_

#include <os_types.h>

#define RES_CONTAINER_NUM_LIMIT_MIN         1
#define RES_CONTAINER_NUM_LIMIT_MAX         INT_MAX
#define RES_CONTAINER_NUM_LIMIT_DEFAULT     1000
#define RES_CONTAINER_MEM_UNLIMITED         0
#define RES_CONTAINER_MEM_LIMIT_DEFAULT     RES_CONTAINER_MEM_UNLIMITED

#define RES_ID_GENERATE                     0

typedef enum gen_strategy {
    RES_ID_GEN_STRATEGY_NOGEN,      //!< Режим без генерации номеров, только захват    RES_ID_GEN_STRATEGY_INC_AGING   //!< Режим генерации "инкрементный,освободился-не занимается пока нет переполнения"} gen_strategy_t;

typedef struct res_header {
    void *ref;
    int type;
    int reserved;
    const size_t datalen;
} res_header_t;

#define GET_RES_DATA(hdr) ((void *)hdr + sizeof(res_header_t))

typedef void * res_container_t;

/**
 * Инициализация контейнера ресурсов. При этом функция выделяет под контейнер динамическую память по kmalloc и
 * выполняет инициализацию в соответствии с заданными настройками. Контейнеры могут быть двух типов:
 *  - с динамической генерацией номеров ресурсов;
 *  - без генерации номеров, то есть ориентированные на захват заданных номеров.
 * В результате успешной инициализации контейнера обновляется значение его идентификатора res_container_t.
 *
 * @param container         - идентификатор контейнера
 * @param numlimit          - максимальное число размещенных ресурсов в контейнере
 * @param memlimit          - максимальный суммарный объем памяти всех размещенных ресурсов
 * @param gen_strategy      - тип контейнера и стратегия генерации динамических номеров ресурсов
 * @return  OK                 - выполнено
 *          ERR_ILLEGAL_ARGS   - недопустимые аргументы
 *          ERR                - ошибка
 */
int resm_container_init (res_container_t *container, int numlimit, size_t memlimit, gen_strategy_t gen_strategy);

/**
 * Полное освобождение памяти, занимаемой контейнером и размещенными в нем ресурсами.
 * После выполнения идентификатор контейнера становится недействительным.
 * Перед освобождением памяти каждого ресурса выполняется вызов внешней callback-функции для
 * выполнения подготовительных действий перед удалением.
 * @param container
 * @param res_free
 * @return  >= 0 - число освобожденных ресурсов
 *          ERR  - ошибка
 */
int resm_container_free (res_container_t *container, void (*res_free) (res_container_t *container, int id, struct res_header *hdr));

/**
 * Возвращает суммарный объем памяти, занимаемый контейнером и размещенными в нем ресурсами
 * @return размер памяти в байтах
 */
size_t resm_get_container_memsize ();

/**
 * Размещение ресурса в контейнере с заданным id номером или с генерацией номера, выделение памяти
 * для дополнительных данных ресурса вместе с заголовом res_header. Дополнительные данные
 * доступны по адресу, возвращаемому макросом GET_RES_DATA(hdr). Гарантируется выравнивание размещенных
 * дополнительных данных согласно стандарту для текущей аппаратной платформы (8 байт для ARM EABI).
 * Если контейнер был инициализирован с генерацией номеров, то есть со стратегией отличной от RES_ID_GEN_STRATEGY_NOGEN,
 * то при вызове id должен быть всегда равен RES_ID_GENERATE.
 *
 * После успешного размещения ресурс является заблокированным и прерывания запрещены аналогично
 * выполнению функции search_and_lock. Поэтому необходимо после всех изменений в полях заголовка и дополнительных
 * данных как можно скорее вызвать функцию resm_unlock, которая разблокирует ресурс и восстановит исходное состояние
 * системного флага прерываний до вызова resm_create_and_lock.
 *
 * Важно! Полученной ссылкой на заголовок можно пользоваться только до разблокировки ресурса.
 * После этого она становится устаревшей, если есть конкурирующие места удаления ресурса.
 *
 * @param container             - идентификатор контейнера
 * @param id                    - RES_ID_GENERATE или идентификатор ресурса для захвата
 * @param datalen               - длина дополнительных данных ресурса
 * @param hdr                   - [out] заголовок ресурса
 * @return > 0                  - идентификатор размещенного ресурса
 *         ERR_BUSY             - ресурс с заданным номером уже размещен в контейнере
 *         ERR_ILLEGAL_ARGS     - недопустимые аргументы
 *         ERR_NO_MEM           - превышен заданный для контейнера предел по объему полезной занятой памяти
 */
int resm_create_and_lock (res_container_t *container, int id, size_t datalen, struct res_header **hdr);

/**
 * Поиск ресурса и его блокировка для дальнейшей безопасной работы с запрещенными прерываниями.
 * Разблокировка должна быть выполнена как можно скорее по resm_unlock.
 * Блокировка/разблокировка аналогична kobject_lock и kobject_unlock, то есть выполняется запрет прерываний
 * на время работы с ресурсом. В то же время не блокируется весь контейнер на это время,
 * то есть в SMP системе на другом ядре может быть выполнен захват и работа с другим ресурсом контейнера.
 *
 * Важно! Полученной ссылкой на заголовок можно пользоваться только до разблокировки ресурса.
 * После этого она становится устаревшей, если есть конкурирующие места удаления ресурса.
 *
 * @param container                 - идентификатор контейнера
 * @param id                        - идентификатор ресурса
 * @param hdr                       - [out] заголовок ресурса
 * @return  OK                      - ресурс найден и заблокирован для работы с ним
 *          ERR_ILLEGAL_ARGS        - не допустимые аргументы
 *          ERR                     - ресурс не найден
 */
int resm_search_and_lock (res_container_t *container, int id, struct res_header **hdr);

/**
 * Разблокировка ресурса в контейнере с восстановлением флага прерываний до вызова search_and_lock или create_and_lock
 * @param container                 - идентификатор контейнера
 * @param hdr                       - заголовок ресурса
 * @return      OK                  - ресурс удален
 *              ERR_ILLEGAL_ARGS    - не допустимые аргументы
 */
int resm_unlock (res_container_t *container, struct res_header *hdr);

/**
 * Удаление ресурса по ссылке. Выполняется с блокировкой ресурса на бесконечном ожидании.
 * Вызов этой функции допускается только если ресурс не блокирован ранее текущим ядром процессора.
 * Контроль этого факта внутри не выполняется.
 * @param container                 - идентификатор контейнера
 * @param id                        - идентификатор ресурса
 * @return      OK                  - ресурс удален
 *              ERR_ILLEGAL_ARGS    - не допустимые аргументы
 */
int resm_remove (res_container_t *container, struct res_header *hdr);

/**
 * Удаление ресурса, блокированного текущим ядром процессора.
 * Применяется после выполнения resm_create_and_lock или resm_search_and_lock там, где это необходимо.
 * При этом выполняется восстановление флага прерываний аналогично выполнению resm_unlock.
 * @param container                 - идентификатор контейнера
 * @param id                        - идентификатор ресурса
 * @return      OK                  - ресурс удален
 *              ERR_ILLEGAL_ARGS    - не допустимые аргументы
 */
int resm_remove_locked (res_container_t *container, struct res_header *hdr);

/**
 * Удаление ресурса по номеру. Выполняется поиск с блокировкой ресурса на бесконечном ожидании.
 * Вызов этой функции допускается только если ресурс не блокирован ранее текущим ядром процессора.
 * Контроль этого факта внутри не выполняется.
 * @param container                 - идентификатор контейнера
 * @param id                        - идентификатор ресурса
 * @return      OK                  - ресурс удален
 *              ERR_ILLEGAL_ARGS    - не допустимые аргументы
 *              ERR                 - ресурс не найден
 */
int resm_search_remove (res_container_t *container, int id);

static inline void resm_set_ref (struct res_header *hdr, void *ref)
{
    hdr->ref = ref;
}

static inline void * resm_get_ref (struct res_header *hdr)
{
    return hdr->ref;
}

static inline void resm_set_type (struct res_header *hdr, int type)
{
    hdr->type = type;
}

static inline int resm_get_type (struct res_header *hdr)
{
    return hdr->type;
}

static inline size_t resm_get_datalen (struct res_header *hdr)
{
    return hdr->datalen;
}

static inline void * resm_get_data (struct res_header *hdr)
{
    return hdr++;
}

#endif /* RESM_H_ */
