#ifndef RTHEAP_H_
#ifdef __cplusplus
extern "C" {
#endif
#define RTHEAP_H_

#include <os_types.h>
#include <stdint.h>
#include <stddef.h>
/**
 * Выравнивание данных в куче всегда должно быть степенью двойки.
 * По спецификации ARM EABI выравнивание по умолчанию должно быть кратно двойному слову
 * (8 байт для 32-й системы), это необходимо для поддержки выравнивания данных размером
 * в двойное слово и выравнивания стека,
 * то есть используем
 */
#define HEAP_DEFAULT_ALIGN        (sizeof(size_t) << 1)

typedef union {
    int32_t val;
    struct {
        int32_t autoextend_freesize :24;
        int32_t rtenabled           :1;
        int32_t alloc_firstfit      :1;
    };
} heap_flags_t;

enum err heap_init (void *heap, size_t size, heap_flags_t mode,
        void *(*getNewHeapRegion) (size_t *size_io));
enum err heap_region_add (void *heap, void *start, size_t size);
void *heap_alloc (void *heap, size_t size);
void *heap_alloc_aligned (void *heap, size_t size, size_t align);
enum err heap_free (void *heap, void *ptr);

enum err heap_get_status (void *heap);
size_t heap_get_freesize (void *heap);
size_t heap_get_size (void *heap);

#ifdef __cplusplus
}
#endif
#endif /* RTHEAP_H_ */
