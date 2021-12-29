/** \brief Виртуальная память.

 Используется прямой режим работы виртуальной памяти (VA = PA).

 Выделение и работа памяти только постранично.
 Для управления страницами используется страничный аллокатор.
 Память разбита на сегменты (фрагменты памяти укладывающиеся на страницы).
 Сегменты организованы в карты памяти.

// TODO почему все так сделано - ядро-либа!

*/

#ifndef VM_H
#define VM_H

#include <os_types.h>
#include <rbtree.h>
#include <proc.h>

/* Описатель директории таблицы */
struct pgd {
    uint32_t entry;  // для быстрой загрузки в таблицу директорий
    //uint32_t kentry; // для обратной загрузки директории пересекающихся со страницами ядра
    struct pgd *kpgd;
    uint32_t cnt;    // кол-во использованных страниц в директории
    uint32_t *pgt;   // для простого доступа в таблице страниц директории
};

/** типы выделяемых сегментов */
enum seg_type {
    SEG_NORMAL, //!< обыкновенный
    SEG_STACK,  //!< стек с защитными страницами с обеих сторон
};

/** карта памяти */
struct mmap {
    kobject_lock_t lock;
    int mmu_pool;           // Индекс пула
    struct rb_tree *pgds;   // Дерево записей для корректировки L1 при загрузке
    struct rb_tree *segs;
    size_t size;
};

/** сегмент */
struct seg {
    void *adr;
    struct mmap *map;
    enum seg_type type : 8;
    unsigned long : 24;
    size_t size;
    mem_attributes_t attr;
    int ref;
    struct rb_tree *shared;
};

/*! карта памяти ядра */
#define kmap kproc.mmap

/** \brief Инициализация работы виртуальной памяти.
 * Запускается при запуске системы. */
void vm_init ();

/** \brief Включение виртуальной памяти.
 * Карта памяти ядра должна быть уже сформирована и загружена. */
void vm_enable ();

/** \brief Создание карты памяти. */
struct mmap* vm_map_create ();

/** \brief Уничтожение карты памяти.
 * После этого вызова вся память карты будет обязательно освобождена. */
void vm_map_terminate (struct mmap *map);

#define COMPLETE  2
#define PARTIAL   1
#define DISJOINT  0

/** \brief Проверка нахождения памяти в карте
 *
 * \warning Реализация функции по запросу Игоря
 * комментарий обязателен и не должен быть удален!
 *
 * \retval DISJOINT  //!< полное непопадание
 * \retval PARTIAL   //!< частичное попадание
 * \retval COMPLETE  //!< полное попадание
 * */
int vm_map_lookup (struct mmap *map, void *adr, size_t size);

/** \brief Переключение карты памяти */
void vm_switch (struct mmap *from, struct mmap *to);

/** \brief Динамическое выделение памяти.
 *
 * \param map    Карта памяти, в которую выделяется память
 * \param pages  Запрашиваемое кол-во страниц памяти
 * \param attr   Атрибуты запрашиваемой памяти
 *
 * \return Указатель на выделенную память
 * \retval NULL Память не выделена
 *  */
void* vm_alloc (struct process *proc, size_t pages, mem_attributes_t attr);

/** \brief Динамическое выделение выравненной памяти.
 *
 * \param map    Карта памяти, в которую выделяется память
 * \param pages  Запрашиваемое кол-во страниц памяти
 * \param aling  Запрашиваемое выравнивание
 * \param attr   Атрибуты запрашиваемой памяти
 *
 * \return Указатель на выделенную память
 * \retval NULL Память не выделена
 * */
void* vm_alloc_align (struct mmap *map, size_t pages, size_t align, mem_attributes_t attr);

/** \brief Выделение памяти под стек.
 *
 * \param map              Карта памяти, в которую выделяется память
 * \param pages            Запрашиваемое кол-во страниц памяти
 * \param only_kernel      Доступная только ядру память
 *
 * \return Указатель на выделенную память
 * \retval NULL Память не выделена
 * */
void* vm_alloc_stack (struct mmap *map, size_t pages, bool only_kernel);

/** \brief Выделение памяти по фиксированному адресу.
 *
 * \param map    Карта памяти, в которую выделяется память
 * \param adr    Адрес начала памяти
 * \param pages  Запрашиваемое кол-во страниц памяти
 * \param attr   Атрибуты запрашиваемой памяти
 *
 * \return Ошибки выполнения
 * */
int vm_map (struct process *proc, void *adr, size_t pages, mem_attributes_t attr);

/** \brief Освобождение памяти. */
int vm_free (struct process *proc, void *adr);

/** \brief Создать сегмент в карте процесса. */
struct seg* seg_create (struct mmap* map, void *adr, size_t size, mem_attributes_t attr);

/** \brief Получить указатель на сегмент. */
struct seg* vm_seg_get (struct mmap *map, void *adr);

/** \brief Перемещение сегмента между карт памяти. */
int vm_seg_move (struct mmap *map_from, struct mmap *map_to, struct seg *seg);

/** \brief Расшаривание сегмента. */
int vm_seg_share (struct mmap *map_to, struct seg *seg);

/** \brief Возврат расшаренного сегмена. */
int vm_seg_unshare (struct mmap *map_from, struct seg *seg);

/** \brief Получение информации о памяти системы. */
void vm_get_info (struct mem_info *info);

void map_kmap (struct process *proc, void *adr);
void unmap_kmap (struct process *proc, void *adr);

void mmap (struct mmap *map, const struct seg *seg);

size_t print_map (char *buf, size_t max, struct mmap *map);

#endif

