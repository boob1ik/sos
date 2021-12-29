/*! \brief Система управления выделением страниц памяти. */

#ifndef PAGE_H
#define PAGE_H

#include <os_types.h>

/** \brief Инициализация управления страницами.
 *
 * \param page_size  Размер страниц */
void page_allocator_init (size_t page_size);

/** \brief Добавление страниц на управление.
 *
 * \param start         Адрес памяти
 * \param num           Кол-во добавляемых страниц
 * \param static_alloc
 *
 * \return Ошибки выполнения
 * */
int page_allocator_add (void *start, uint32_t num, bool fixed);

/** \brief Динамическое выделение памяти.
 *
 * \param pages    Запрашиваемое кол-во страниц памяти
 *
 * \return Указатель на выделенную память
 * \retval NULL Память не выделена
 * */
void* page_alloc (uint32_t n);

/** \brief Динамическое выделение памяти с выравниваем.
 *
 * \param pages     Запрашиваемое кол-во страниц памяти
 * \param align     Заданное выравнивание в байтах
 * \param realtime  1 - поиск блока двойного размера и возврат ошибки если такого нет
 *                  0 - попытка разместить выровненный блок в любом блоке большего размера - если такого нет - PANIC
 *
 * \return Указатель на выделенную память
 * \retval NULL Память не выделена
 *  */
void* page_alloc_align (uint32_t pages, uint32_t align, uint32_t realtime);

/** \brief Выделение памяти по фиксированному адресу.
 *
 * \param adr    Адрес начала памяти
 * \param pages  Запрашиваемое кол-во страниц памяти
 * \param multiple_access   Выделяемая память может выделяться повторно
 *
 * \return Указатель на выделенную память
 * \retval NULL Память не выделена
 * */
void* page_alloc_fixed (size_t adr, uint32_t pages, bool multiple_access);

/** \brief Освобождение памяти. */
void page_free (void *adr);

size_t get_page_total ();
size_t get_page_free ();
size_t get_page_used ();

size_t get_page_fixed_total ();
size_t get_page_fixed_free ();
size_t get_page_fixed_used ();

size_t get_page_dynamic_total ();
size_t get_page_dynamic_free ();
size_t get_page_dynamic_used ();

#endif
