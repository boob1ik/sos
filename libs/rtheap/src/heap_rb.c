/** \brief Библиотека управления динамической памятью реального времени,
 версия для 32-х разрядной системы.
 Предназначена для управления множественными кучами динамической памяти.
 Каждая куча должна быть инициализирована
 и включает заголовок struct heap_rb. По каждой операции выделения и освобождения памяти
 ведется контроль целостности состояния кучи.
 Куча состоит из произвольного набора неперекрывающихся регионов памяти,
 между которыми допускаются разрывы в адресном пространстве.
 Библиотека предусматривает только возможность роста числа регионов кучи за счет
 получения новых регионов от операционной системы(страничной памяти)
 или какими-либо другими средствами (настраивается).
 Основным режимом работы является взаимодействие библиотеки с операционной системой
 для динамического наращивания размера кучи по мере необходимости.
 Это выполняется автоматически с помощью получения страничной памяти у ОС.
 Операционная система имеет возможности по ограничению роста кучи для каждого процесса.
 Она также решает задачу освобождения страничной памяти процесса после его завершения.
 Реализация кучи предусматривает защиту от рекурсивного вызова метода получения
 новых регионов памяти.

 В текущей версии на уровне модуля не предусмотрены механизмы дефрагментации и освобождения
 регионов, полученных при автоматическом расширении общего размера кучи.
 Куча строится на базе 2-х красно-черных деревьев всех блоков памяти по адресам и
 свободных блоков по размерам. Алгоритм кучи при наличии флага heap_flags_t.rtenabled
 гарантирует логарифмическую скорость выполнения операций с кучей. Без установленного флага
 алгоритм обеспечивает более эффективное использование регионов кучи в условиях специальных
 требований по выравниванию запрашиваемых блоков памяти (при значениях выравнивания
 > HEAP_DEFAULT_ALIGN), при этом операции могут выполняться за время O(M), где M -
 число всех свободных блоков размером больше или равным требуемому.
 Для 32-х разрядной системы минимальный выделяемый фрагмент памяти равен 32 байтам
 включая 16 байт служебной информации,
 что согласуется с современными подсистемами кэширования внешней памяти со строкой кэша в 32 байта.
 По сравнению с алгоритмом концевых меток (используется в стандартных библиотеках, например libc),
 где минимальный размер фрагмента равен 16 байтам включая 8 байт служебной информации,
 наш алгоритм расчитан на работу в реальном времени и выполняет операции выделения
 и освобождения памяти в худшем случае (при максимальной степени фрагментации) за время O(lg N),
 где N - общее число свободных и занятых фрагментов памяти кучи
 Подробнее об алгоритме см. методы
 */
#include <rtheap.h>
#include <rbtree.h>
#include "rbtree4heap.h"

enum {
    CHUNK_EMPTY,
    CHUNK_ALLOCATED
};

typedef union {
    heap_flags_t val;
    struct {
        int32_t autoextend_freesize :24;
        int32_t rtenabled :1;
        int32_t alloc_firstfit :1;
        int32_t getnewregion_lock :1;
    };
} heap_flags_t_static;

#define HEAP_RB_MAGIC   (0x48524242) // "HRBB"
struct heap_rb {
    size_t magic;
    heap_flags_t_static flags;
    size_t heapsize;
    size_t heapfreesize;
    size_t regions_cnt;
    enum err hstatus;
    struct rb_tree allchunks;
    struct rb_tree4heap freechunks;
    void *(*getNewHeapRegion) (size_t *size);
    size_t heap_lock;
    size_t csum;
};

static size_t calcHeapSum (void *heap)
{
    struct heap_rb *hp = (struct heap_rb *) heap;
    size_t sum = 0;
    sum += hp->magic;
    sum += hp->heap_lock;
    sum += hp->heapsize;
    sum += hp->heapfreesize;
    sum += hp->regions_cnt;
    sum += hp->allchunks.nodes;
    sum += hp->freechunks.nodes;
    return sum;
}

static enum err checkHeapStateConsistency (void *heap)
{
    if (heap == NULL)
        return ERR_ILLEGAL_ARGS;
    if (((struct heap_rb *) (heap))->csum != calcHeapSum(heap))
        return ERR;
    return OK;
}

enum err heap_get_status (void *heap)
{
    enum err res = checkHeapStateConsistency(heap);
    if (res != OK) {
        return res;
    }
    return ((struct heap_rb *) heap)->hstatus;
}

size_t heap_get_freesize (void *heap)
{
    return ((struct heap_rb *) heap)->heapfreesize;
}

size_t heap_get_size (void *heap) {
    return ((struct heap_rb *) heap)->heapsize;
}

enum err heap_init (void *start, size_t size, heap_flags_t flags,
        void *(*allocRegion) (size_t *size_io))
{
    if ((((size_t) start) & (HEAP_DEFAULT_ALIGN - 1)) != 0) {
        return ERR_ILLEGAL_ARGS;
    }
    if (size < sizeof(struct heap_rb) + sizeof(struct rb_node4heap)) {
        return ERR_NO_MEM;
    }
    struct heap_rb *heap = (struct heap_rb *) start;
    heap->magic = HEAP_RB_MAGIC;
    heap->flags.val = flags;
    heap->flags.getnewregion_lock = 0;
    heap->heap_lock = 1;
    heap->heapsize = 0;
    heap->heapfreesize = 0;
    heap->regions_cnt = 0;
    heap->hstatus = OK;
    heap->getNewHeapRegion = allocRegion;
    rb_tree_init(&heap->allchunks);
    rb_tree_set_mode(&heap->allchunks, RBTREE_BY_PTR_ADDRESS);
    hrb_tree_init(&heap->freechunks);
    heap->csum = calcHeapSum(start);
    void *region = (void *) (((size_t) start + sizeof(struct heap_rb) +
    HEAP_DEFAULT_ALIGN - 1) & -HEAP_DEFAULT_ALIGN);
    size_t regsize = (start + size) - region;
    regsize &= -HEAP_DEFAULT_ALIGN;
    return heap_region_add(start, region, regsize);
}

enum err heap_region_add (void *heap, void *start, size_t size)
{
    // проверка аргументов
    if ((((size_t) start) & (HEAP_DEFAULT_ALIGN - 1)) != 0) {
        return ERR_ILLEGAL_ARGS;
    }
    if (size < sizeof(struct rb_node4heap)) {
        return ERR_ILLEGAL_ARGS;
    }
    if ((((size_t) size) & (HEAP_DEFAULT_ALIGN - 1)) != 0) {
        return ERR_ILLEGAL_ARGS;
    }
    // проверяем целостность состояния кучи на каждый вызов API кучи
    enum err res = checkHeapStateConsistency(heap);
    if (res != OK) {
        ((struct heap_rb *) heap)->hstatus = res;
        return res;
    }
    // проверим, что в куче нет регионов перекрывающихся с запрошенным
    // и добавим регион к куче, то есть инициализируем объект в 2х деревьях
    struct rb_node *node = NULL;
    struct rb_node4heap *hnode = NULL;
    struct heap_rb *hp = (struct heap_rb *) heap;

    // перед началом операций с кучей увеличиваем heap_lock
    hp->heap_lock++;

    node = rb_tree_search_neareqlarger(&hp->allchunks, (size_t) start);
    if ((node != NULL)
            && ((((size_t) node) + node->key) < ((size_t) start + size))) {
        // целостность кучи не нарушена, указанный регион не допустим
        hp->csum = calcHeapSum(heap);
        return ERR;
    }
    node = rb_tree_search_nearless(&hp->allchunks, (size_t) start);
    if ((node != NULL) && ((((size_t) node) + node->key) > ((size_t) start))) {
        // целостность кучи не нарушена, указанный регион не допустим
        hp->csum = calcHeapSum(heap);
        return ERR;
    }
    node = (struct rb_node *) start;
    hnode = (struct rb_node4heap *) start;
    rb_node_init(node);
    rb_node_set_key(node, size);
    hrb_node_init((struct rb_node4heap *) start);
    node->user_bit = CHUNK_EMPTY;
    rb_tree_insert(&hp->allchunks, node);
    hrb_tree_insert(&hp->freechunks, hnode);
    hp->regions_cnt++;
    hp->heapsize += size;
    hp->heapfreesize += size;
    hp->csum = calcHeapSum(heap);
    return res;
}

/**
 * Метод захвата блока памяти с заданным параметром выравнивания.
 * Рекомендуется использовать всегда HEAP_DEFAULT_ALIGN, если нет специальных требований
 * по выравниванию. Для захвата памяти с крупным выравниванием на страницы или 512, 1К, 2К,
 * крайне рекомендуется применять соответствующие системные вызовы операционной системы
 * постраничного захвата памяти и дополнительную функциональную обработку где это необходимо.
 *
 * Алгоритм захвата блока памяти заданного размера и с заданным выравниванием:
 * 1) Аргумент align приводится к минимальному равному степени 2, не менее HEAP_DEFAULT_ALIGN
 * 2) Осуществляется поиск свободного блока в дереве по размерам свободных блоков
 * с учетом требований выравнивания. Свободные блоки, несмотря на подходящий размер, могут
 * не подходить для захвата с учетом заданных требований по выравниванию, то есть не
 * могут использоваться в прямую или быть разбиты на свободный блок-остаток и занятый блок
 * заданного размера. Поэтому, если выравнивание > HEAP_DEFAULT_ALIGN(предполагается, что
 * все блоки выравнены на HEAP_DEFAULT_ALIGN или кратное значение степеней двойки), то
 * a) В дереве свободных блоков ищется блок удвоенного размера или блок удвоенного выравнивания
 *  (в зависимости что больше). Если такой блок найден, то он разбивается на свободный
 *  остаток(должен быть не менее sizeof(struct rb_node4heap) и выравнен по умолчанию) и блок,
 *  на пользовательские данные которого возвращается указатель
 * b) Если блока удвоенного размера нет, осуществляется сканирование всех свободных блоков >=
 *   заданного размера и проверка их совместимости с заданным параметром выравнивания
 *   Данный этап будет выполняться за время O(M), где M - число свободных блоков
 *   >= заданного размера. Поэтому, строго говоря, алгоритм
 *   не подходит для применения в системе реального времени когда в куче включен режим
 *   полного сканирования для выделения блока с заданным параметром
 *   выравнивания > HEAP_DEFAULT_ALIGN. Используем флаг кучи heap_flags_t.rtenabled
 *   для регулирования поддержки реального времени, то есть использования пункта b) алгоритма
 *   для сканирования. Когда указанный флаг включен, выполняется проверка только первого
 *   свободного блока достаточного размера, после чего выполняется переход на 3), если условие
 *   выравнивания не может быть выполнено в этом блоке
 * 3) При отсутствии свободного блока и не пустой ссылки на метод получения новых регионов
 * памяти выполняется попытка двукратного увеличения размера кучи (или двукратного размера
 * запрошенного блока, в зависимости от того, что больше). Далее, выполняются повторно действия
 * по п.2), если увеличение кучи выполнено успешно. Неограниченный рост кучи процесса
 * может блокироваться средствами операционной системы или внешнего программного модуля
 */
void *heap_alloc_aligned (void *heap, size_t size, size_t align)
{
    struct heap_rb *hp = (struct heap_rb *) heap;
    // проверяем целостность состояния кучи на каждый вызов API кучи
    void *res = NULL;
    enum err e = checkHeapStateConsistency(heap);
    if (e != OK) {
        ((struct heap_rb *) heap)->hstatus = e;
        return res;
    }
    // перед началом операций с кучей увеличиваем heap_lock
    hp->heap_lock++;

    size_t workalign = align;
    if (workalign == 0) {
        workalign = HEAP_DEFAULT_ALIGN;
    } else {
        // минимальное выравнивание должно быть всегда HEAP_DEFAULT_ALIGN
        workalign = (workalign + HEAP_DEFAULT_ALIGN - 1)
                & (-HEAP_DEFAULT_ALIGN);
        // выравнивание только по степени двойки, берем крайний справа бит
        // (наименьший шаг выравнивания)
        workalign = workalign & (-workalign);
    }
    size_t allocsize =
            (size < MIN_CHUNK_USER_SIZE) ?
                    sizeof(struct rb_node4heap) :
                    size + ALLOCATED_CHUNK_HDR_SIZE;
    // необходимо всегда выравнивать хвост блока на HEAP_DEFAULT_ALIGN
    // делаем это кратным HEAP_DEFAULT_ALIGN размером
    allocsize += HEAP_DEFAULT_ALIGN - 1;
    allocsize &= -HEAP_DEFAULT_ALIGN;

    size_t searchsize, tmp, tmp2, tmp3, delta;
    if (workalign > HEAP_DEFAULT_ALIGN) {
        if (workalign <= allocsize) {
            searchsize = allocsize << 1;
        } else {
            searchsize = workalign << 1;
        }
        if (searchsize == 0) {
            // проверка на переполнение
            searchsize = allocsize;
        }
    } else {
        searchsize = allocsize;
    }

    struct rb_node4heap *node;

    if (hp->flags.alloc_firstfit) {
        node = hrb_tree_search_eqlarger(&hp->freechunks, searchsize);
    } else {
        node = hrb_tree_search_neareqlarger(&hp->freechunks, searchsize);
    }
    struct rb_node *left = NULL, *newnode = NULL;

    if ((node == NULL) && (workalign > HEAP_DEFAULT_ALIGN)) {
        // в условиях наличия специальных требований по выравниванию и
        // отсутствия подходящих блоков двойного размера меняем стратегию поиска
        // на просмотр хотя бы одного свободного блока размером allocsize
        searchsize = allocsize;
        if (hp->flags.alloc_firstfit) {
            node = hrb_tree_search_eqlarger(&hp->freechunks, searchsize);
        } else {
            node = hrb_tree_search_neareqlarger(&hp->freechunks, searchsize);
        }
    }
    struct rb_node4heap *listrootnode = node;
    while (node != NULL) {
        // предполагаемое начало пользовательских данных блока с учетом выравнивания
        tmp = (((size_t) &(node->up_color)) + workalign - 1) & (-workalign);

        if (tmp + size > ((size_t) node) + node->key) {
            // не влезаем в блок
            if (hp->flags.rtenabled) {
                break;
            }
            node = hrb_tree_get_neareqlarger(listrootnode, node);
            if ( (node != NULL) && (node->user_bit == 0) ) {
                listrootnode = node;
            }
            continue;
        } else {
            // найден блок нужного размера, проверим его более детально
            // Внимание! Блок может не подойти,
            // если слева нет примыкающих блоков и выравнивание не позволяет
            // создать слева новый свободный блок
            tmp2 = tmp - ((size_t) &(node->up_color));
            if (tmp2 != 0) {
                // начало пользовательских данных узла node не выравнено,
                // решаем вопросы с левым остатком:
                if (tmp2 < MIN_CHUNK_SIZE) {
                    //  слева нет места для создания свободного блока,
                    // попробуем сдвинуть данные правее
                    // чтобы создать слева свободный блок
                    tmp3 = (((size_t) &(node->up_color)) +
                    MIN_CHUNK_SIZE + workalign - 1) & (-workalign);

                    if (tmp3 + size > ((size_t) node) + node->key) {
                        // не получилось, попробуем
                        // присоединить остаток слева к занятому блоку если он есть
                        // и примыкает без пропусков к найденному свободному
                        left = rb_tree_get_nearless((struct rb_node *) node);
                        if ((left != NULL)
                                && (((size_t) left) + left->key
                                        == ((size_t) node))) {
                            // присоединяем простым добавлением размера
                            // без влияния на само дерево
                            delta = tmp - ALLOCATED_CHUNK_HDR_SIZE
                                    - ((size_t) left) - left->key;
                            left->key += delta;
                            hp->heapfreesize -= delta;
                        } else {
                            // не можем использовать блок с заданным параметром выравнивания
                            if (hp->flags.rtenabled) {
                                break;
                            }
                            node = hrb_tree_get_neareqlarger(listrootnode,
                                    node);
                            if ( (node != NULL) && (node->user_bit == 0) ) {
                                listrootnode = node;
                            }
                            continue;
                        }
                    } else {
                        tmp = tmp3;
                    }
                }
                tmp3 = ((size_t) node) + node->key;
                // найдено решение по использованию блока,
                // исключаем его из свободных
                hrb_tree_remove(&hp->freechunks, node);
                if (left == NULL) {
                    // текущий свободный блок будет меньше
                    // и он остается в основном дереве по адресам
                    delta =
                            node->key
                                    - (tmp - ALLOCATED_CHUNK_HDR_SIZE
                                            - ((size_t) node));
                    node->key -= delta;
//                    hp->heapfreesize -= delta;
                    hrb_node_init(node);
                    hrb_tree_insert(&hp->freechunks, node);
                } else {
                    // блока с таким адресом уже не будет, остаток присоединен к левому занятому
                    rb_tree_remove(&hp->allchunks, (struct rb_node *) node);
                }
                newnode = (struct rb_node *) (tmp - ALLOCATED_CHUNK_HDR_SIZE);
                rb_node_init(newnode);
                rb_tree_insert(&hp->allchunks, newnode);
            } else {
                // блок точно можно использовать, исключаем из свободных
                tmp3 = ((size_t) node) + node->key;
                hrb_tree_remove(&hp->freechunks, node);
                newnode = (struct rb_node *) node;
            }
            // теперь решаем вопросы с правым остатком если он есть
            // здесь newnode - наш новый занятый блок
            newnode->user_bit = CHUNK_ALLOCATED;
            newnode->key = allocsize;
            tmp2 = tmp3 - ((size_t) newnode) - allocsize;
            if (tmp2 != 0) {
                // есть правый остаток
                if (tmp2 < MIN_CHUNK_SIZE) {
                    // присоединяем его к новому занятому блоку
                    newnode->key += tmp2;
                } else {
                    // создаем новый блок свободного остатка справа
                    node = (struct rb_node4heap *) ((size_t) newnode
                            + newnode->key);
                    node->key = tmp2;
//                    hp->heapfreesize += tmp2;
                    rb_node_init((struct rb_node *) node);
                    hrb_node_init(node);
                    ((struct rb_node *) node)->user_bit = CHUNK_EMPTY;
                    rb_tree_insert(&hp->allchunks, (struct rb_node *) node);
                    hrb_tree_insert(&hp->freechunks, node);
                }
            }
            hp->heapfreesize -= newnode->key;
            res = &(newnode->data);
            break;
        }
    }
    if ((hp->getNewHeapRegion != NULL)
            && ((res == NULL)
                    || (hp->heapfreesize < hp->flags.autoextend_freesize))) {
        // нарастим кучу и повторим захват рекурсивно
        size_t newsize =
                (hp->heapsize > searchsize) ?
                        (hp->heapsize << 1) : (searchsize << 1);

        // все действия с кучей завершены, обновим контрольную сумму
        hp->csum = calcHeapSum(heap);

        if (newsize == 0) {
            // переполнение адреса по алгоритму двукратного увеличения
            newsize = searchsize << 1;
        }
        if (newsize == 0) {
            // переполнение адреса даже на двойной запрошенный размер
            return res; // нет памяти
        }
        // критическая секция защиты от рекурсии вызова getNewHeapRegion,
        // если в ней присутствуют вызовы heap_alloc
        if (!hp->flags.getnewregion_lock) {
            hp->flags.getnewregion_lock = 1;
            void *r = hp->getNewHeapRegion(&newsize);
            hp->flags.getnewregion_lock = 0;
            if (r != NULL) {
                heap_region_add(heap, r, newsize);
                if (res == NULL) {
                    // после наращивания кучи пробуем повторить попытку выделения памяти
                    // только если в этом вызове попытка была не удачной
                    return heap_alloc_aligned(heap, size, align);
                }
            }
        }
    } else {
        // все действия с кучей завершены, обновим контрольную сумму
        hp->csum = calcHeapSum(heap);
    }
    if (res == NULL) {
        hp->hstatus = ERR_NO_MEM;
    }
    return res;
}

void *heap_alloc (void *heap, size_t size)
{
    return heap_alloc_aligned(heap, size, HEAP_DEFAULT_ALIGN);
}

enum err heap_free (void *heap, void *ptr)
{
    // проверяем целостность состояния кучи на каждый вызов API кучи
    enum err res = checkHeapStateConsistency(heap);
    if (res != OK) {
        ((struct heap_rb *) heap)->hstatus = res;
        return res;
    }
    struct heap_rb *hp = (struct heap_rb *) heap;

    // перед началом операций с кучей увеличиваем heap_lock
    hp->heap_lock++;

    struct rb_node *node = rb_tree_search(&hp->allchunks, (size_t) ptr);
    if ((node == NULL) || (node->user_bit != CHUNK_ALLOCATED)) {
        // блок по такому адресу не зарегистрирован
        hp->csum = calcHeapSum(heap);
        return ERR;
    }
    size_t s = node->key;

    // выполняем слияние если это необходимо слева и справа с существующими свободными блоками,
    // левый или целевой блок оставляем в основном дереве по адресам,
    // меняем только размер блока и включаем его в дерево свободных
    struct rb_node *left = rb_tree_get_nearless(node);
    struct rb_node *right = rb_tree_get_nearlarger(node);
    struct rb_node4heap *freenode = (struct rb_node4heap *) node;
    if (!((left != NULL) && (((size_t) left) + left->key == ((size_t) node))
            && (left->user_bit == CHUNK_EMPTY))) {
        left = NULL;
    } else {
        rb_tree_remove(&hp->allchunks, node);
        hrb_tree_remove(&hp->freechunks, (struct rb_node4heap *) left);
        freenode = (struct rb_node4heap *) left;
    }
    if (!((right != NULL) && (((size_t) node) + node->key == ((size_t) right))
            && (right->user_bit == CHUNK_EMPTY))) {
        right = NULL;
    } else {
        rb_tree_remove(&hp->allchunks, right);
        hrb_tree_remove(&hp->freechunks, (struct rb_node4heap *) right);
    }
    if (left != NULL) {
        freenode->key += node->key;
    }
    if (right != NULL) {
        freenode->key += right->key;
    }
    ((struct rb_node *) freenode)->user_bit = CHUNK_EMPTY;
    hrb_node_init(freenode);
    hrb_tree_insert(&hp->freechunks, freenode);
    hp->heapfreesize += s;
    hp->csum = calcHeapSum(heap);
    return OK;
}
