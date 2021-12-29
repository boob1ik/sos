/** \brief "Куча" ядра */

#ifndef KMEM_H
#define KMEM_H

#include <os_types.h>

/** \brief Инициализация кучи.
 * Запускается при запуске системы.
 * Начальный размер кучи задается скритом линкера.
 * По мере необходимости куча автоматически расширяется в динамическую память
 * системы. */
void kmem_init ();

/** \brief Выделение памяти из кучи. */
void* kmalloc (size_t size);

/** \brief Выделение памяти из кучи с выравниванием. */
void* kmalloc_aligned (size_t size, size_t align);

/** \brief Освобождение памяти. */
void kfree (void *ptr);

/** \brief Получить размер кучи. */
size_t get_kheap_size ();

/** \brief Получить свободный размер кучи. */
size_t get_kheap_free_size ();

#define HEAP_ATTR \
    .shared = MEM_SHARED_OFF, \
    .exec = MEM_EXEC_NEVER, \
    .type = MEM_TYPE_NORMAL, \
    .inner_cached = MEM_CACHED_WRITE_BACK_WRITE_ALLOCATE, \
    .outer_cached = MEM_CACHED_WRITE_BACK_WRITE_ALLOCATE, \
    .process_access = MEM_ACCESS_NO, \
    .os_access = MEM_ACCESS_RW, \


// запрет авторасширения
void kheap_ext_lock ();

// разрешение авторасширения
void kheap_ext_unlock ();

int print_kheap (char *buf);

#endif
