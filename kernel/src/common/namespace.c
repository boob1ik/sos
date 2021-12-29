#include <arch.h>
#include "namespace.h"
#include <mem/kmem.h>
#include <syn/ksyn.h>
#include <rbtree.h>
#include <string.h>

/**
 *  Модуль управления корневым пространством имен, представляющим собой дерево с промежуточными узлами-папками
 *  и конечными узлами-листьями, которые содержат тип объекта и ссылку на него.
 *  Каждый узел именован. Путь до листа представляется в виде строки с именами узлов, разделяемыми символом '\'.
 *  Путь начинается с корня, который обозначается также '\' и может не указываться в явном виде в начале строки.
 *  Промежуточные узлы-папки создаются и удаляются автоматически и являются частью алгоритма управления пространством.
 *  Каждый узел в пространстве имен представляем структурой struct nsnode.
 *  Узлы могут быть двух внутренних типов: папка и объект.
 *  Папки включают поддерево (к-ч) nsnode.subtree, а объекты его не имеют (пустая ссылка - признак отличия).
 *  Для папок nsnode.info не содержит полезных данных и является таким образом накладными расходами,
 *  однако они малы и допустимы в силу много меньшего числа папок по сравнению с объектами.
 *  Такой подход позволяет создать удобный API модуля и инкапсулировать внутреннюю его работу,
 *  сохраняя высокие показатели производительности за счет минимизации динамического выделения памяти через kmalloc.
 *  Каждый узел имеет хэш-значение, соответсвующее его имени.
 *  Таким образом, в основе алгоритма корневого пространства имен лежит подсчет 32-битного быстрого хэш-значения
 *  каждого слова пути между разделителями '\'. Подсчет хэш должен иметь достаточно хорошее распределение, быть
 *  быстрым и простым, поэтому выбран алгоритм HashLy. При одинаковых хэш-значениях формируется
 *  двусвязный список узлов и алгоритм поиска переходит из O(Log) в O(N) класс. Однако, вероятность таких исходов
 *  крайне мала учитывая подсчет хэш по словам и среднее малое число букв в слове.
 *
 *  В настоящее время реализация не предусматривает ограничение на число узлов в пространстве с одним значением хэш.
 *  При введении такого ограничения можно говорить о строгом соблюдении алгоритмов жесткого реального времени в модуле,
 *  то есть со сходимостью O(Log)
 *  TODO
 */

typedef enum ns_object_type_internal {
    NS_ANY_OBJECT              = KOBJECT_NUM,
    NS_OBJECT_FOLDER,
    NS_OBJECT_DELETED,
} ns_object_type_internal_t;

typedef struct nsnode {
    struct namespace_node info;
    int type;           //!< тип объекта из множеств ns_object_type_internal и kobject_type_t в config.h
    kobject_lock_t lock;
    struct rb_node *rbn;//! < все узлы добавляются в дерево;rbn=&local_rbn || &samehash_rbn; rbn.ref = struct nsnode*
    struct rb_tree *subtree;
    struct nsnode *parent;
    struct nsnode *prev;//! < двусвязный список узлов с одинаковым хэш-значением слов на данном уровне
    struct nsnode *next;

    struct rb_node local_rbn; //! < обычно каждый узел имеет отдельный соответствующий узел дерева, искл. при равных хэш
    char name[2];       //! < размер строки имени узла не фиксирован, строка следует с этого места и далее
} nsnode_t;

typedef struct namespace_internal {
    nsnode_t root; //!< корневой узел, никогда не удаляется, root.lock применяется для всего пространства
    struct rb_tree root_subtree;
    char root_name[3];
    char separator;
    char alt_separator;
} namespace_internal_t;

/**
 * Функция считает 32-битное хэш-значение по известному алгоритму HashLy за последовательность байт между
 * разделительными символами или до терминального символа строки, начиная с символа по указателю с адресом sptr.
 * Указатель по адресу sptr сдвигается до следующего слова между разделителями \c.
 * Допускаются следующие друг за другом разделители. В этом случае функция возвращает нулевое хэш-значение за
 * два первых разделителя \c и ссылку перемещает на следующий за ними символ.
 * @param sptr
 * @param hash
 * @return OK если хэш вычислено(включая случай двух разделителей подряд), ERR если пустые входные данные
 */
static int HashLy_NextWord (char **sptr, uint32_t *hash, char separator, char alt_separator)
{
    char *s = *sptr;
    if (s == NULL)
        return ERR;
    *hash = 0;
    if ( (*s == separator) || (*s == alt_separator) ) {
        s++;
        *sptr = s;
    }
    for (; *s != '\0'; s++) {
        if ( (*s == separator) || (*s == alt_separator) ) {
            s++;
            if (*s == '\0')
                *sptr = NULL;
            else
                *sptr = s;
            return OK;
        }
        *hash = (*hash * 1664525) + (unsigned char) (*s) + 1013904223;
//        *hash = 1; // для проверки работы алгоритма модуля с одинаковыми значениями хэш
    }
    if (s != *sptr) {
        *sptr = NULL;
        return OK;
    } else {
        *sptr = NULL;
        return ERR;
    }
}

/**
 * Специальная функция подсчета длины подстроки до символа разделения \separator или до конца строки '\0'
 * @param s
 * @return
 */
static int substrlen (char *s, char separator, char alt_separator)
{
    int i = 0;
    while ( (*s != separator) && (*s != alt_separator) && (*s != '\0')) {
        i++;
        s++;
    }
    return i;
}

/**
 * Функция сравнения подстроки и строки на полное совпадение,
 * подстрока заканчивается '\0' или разделителем, вторая - всегда '\0'
 * @param sbs
 * @param s
 * @return
 */
static int substrcmp (char *sbs, char *s, char separator, char alt_separator)
{
    while (*s != '\0') {
        if (*sbs != *s) {
            return ERR;
        }
        s++;
        sbs++;
    }
    if ( (*sbs == separator) || (*sbs == alt_separator) || (*sbs == '\0')) {
        return 0;
    }
    return ERR;
}

/**
 * Функция поиска узла в дереве по его пути, основная рабочая функция алгоритма модуля.
 * Осуществляет возврат промежуточного узла-папки, ниже которого в пространстве имен
 * отсутствуют узлы по указанному пути. Возвращаемое значение хэш не найденного объекта и ссылка на суффикс пути
 * позволяют быстро и эффективно продолжить работу алгоритма по добавлению необходимых данных в пространство имен.
 *
 * @param pathname
 * @param node
 * @param notfound_suffix
 * @param notfound_hash
 * @return OK                - найден объект в пространстве с указанным путем и
 *                             на его узел возвращена ссылка через node
 *         ERR_ACCESS_DENIED - указанный путь не может быть использован для поиска и хранения объета, нарушено
 *                             условие отсутсвия вложения объектов или любая другая ошибка в пути
 *         ERR               - объект по указанному пути не найден, возвращена ссылка на узел через node
 *                             и остаток пути, начиная с которого в node необходимо создать новые узлы
 *
 */
static int local_ns_lookup (namespace_internal_t *ns, char *pathname, struct nsnode **node,
        char **notfound_suffix, uint32_t *notfound_hash)
{
    struct rb_node *rbn;
    uint32_t hash;
    struct nsnode *n = (*node == NULL) ? &ns->root : *node;
    char *s = (*notfound_suffix == NULL) ? pathname : *notfound_suffix;
    int findfirst = (*notfound_suffix == NULL) ? 0 : 1;

    for (;;) {
        *node = n;
        *notfound_suffix = s;
        if (HashLy_NextWord(&s, &hash, ns->separator, ns->alt_separator) == OK) {

            rbn = (n->subtree == NULL) ? NULL : rb_tree_search(n->subtree, hash);
            if (rbn != NULL) {
                n = (struct nsnode *) rb_node_get_data(rbn);
                // Ссылка всегда на последний узел в списке узлов с одинаковым значением хэш.
                // Всегда проверяем на точное совпадение строки, ищем линейно по списку если он есть,
                // расчет на хороший алгоритм хэширования и малую вероятность совпадения хэш или малое
                // количество таких совпадений, TODO можно потом поставить защиту на число таких совпадений
                // чтобы обеспечить гарантированное время с учетом требований к жесткому реальному времени ядра
                while(n != NULL) {
                    if (substrcmp(*notfound_suffix, n->name, ns->separator, ns->alt_separator) == 0) {
                        break;
                    }
                    n = n->prev;
                }
                if (n != NULL) {
                    // найден узел текущего уровня
                    if (s == NULL) {
                        // полное совпадение пути и наличие объекта по нему
                        *node = n;
                        *notfound_suffix = s;
                        return OK;
                    }
                    // s != NULL, - остался суффикс для поиска

                    // если мы выполняем поиск первого встречного в указанном пути объекта заданного типа,
                    // то проверим, является ли n этим объектом
                    if( findfirst ) {
                        switch(n->type) {
                        case NS_OBJECT_FOLDER:
                            continue;
                        case NS_OBJECT_DELETED:
                            *node = n;
                            *notfound_suffix = s;
                            return ERR_ACCESS_DENIED;
                        default:
                            *node = n;
                            *notfound_suffix = s;
                            return OK;
                        }
                    }
                    continue;
                }
            }
            *notfound_hash = hash;
            return ERR;

        } else {
            return ERR_ACCESS_DENIED;
        }
    }
    return ERR_ACCESS_DENIED;
}

/**
 * @param pathname != NULL
 * @param node != NULL
 * @param suffix != NULL, suffix < pathname + PATHNAME_MAX_BUF
 * @return *node
 */
static int local_ns_skip_until (namespace_internal_t *ns, char *pathname, struct nsnode **node, char *suffix)
{
    struct rb_node *rbn;
    uint32_t hash;
    struct nsnode *n = &ns->root;
    char *s = pathname, *s2 = pathname;
    if(s == suffix) {
        return OK;
    }
    while (s < suffix) {
        *node = n;
        if (HashLy_NextWord(&s, &hash, ns->separator, ns->alt_separator) == OK) {

            rbn = (n->subtree == NULL) ? NULL : rb_tree_search(n->subtree, hash);
            if (rbn != NULL) {
                n = (struct nsnode *) rb_node_get_data(rbn);
                // Ссылка всегда на последний узел в списке узлов с одинаковым значением хэш.
                // Всегда проверяем на точное совпадение строки, ищем линейно по списку если он есть,
                // расчет на хороший алгоритм хэширования и малую вероятность совпадения хэш или малое
                // количество таких совпадений, TODO можно потом поставить защиту на число таких совпадений
                // чтобы обеспечить гарантированное время с учетом требований к жесткому реальному времени ядра
                while(n != NULL) {
                    if (substrcmp(s2, n->name, ns->separator, ns->alt_separator) == 0) {
                        break;
                    }
                    n = n->prev;
                }
                s2 = s;
                if (n != NULL) {
                    // найден узел текущего уровня
                    if (s == suffix) {
                        // полное совпадение пути и наличие объекта по нему
                        *node = n;
                        return OK;
                    }
                    continue;
                }
            }
            return ERR;

        } else {
            return ERR_ACCESS_DENIED;
        }
    }
    return ERR_ACCESS_DENIED;
}

void ns_init (namespace_t *ens)
{
    namespace_internal_t *ns = kmalloc(sizeof(namespace_internal_t));
    ns->root.name[0] = ns->root_name[0];
    ns->root.name[1] = ns->root_name[1];
    ns->root.rbn = &ns->root.local_rbn;
    ns->root.subtree = &ns->root_subtree; // папка
    ns->root.parent = NULL;
    ns->root.prev = NULL;
    ns->root.next = NULL;
    kobject_lock_init(&ns->root.lock);
    rb_tree_init(ns->root.subtree);
    ns->separator = NAMESPACE_DEFAULT_SEPARATOR;
    ns->alt_separator = NAMESPACE_DEFAULT_ALT_SEPARATOR;
    *ens = ns;
}

void ns_init_custom (namespace_t *ens, char c1, char c2)
{
    ns_init(ens);
    namespace_internal_t *ns = (namespace_internal_t *)*ens;
    ns->separator = c1;
    ns->alt_separator = c2;
}

int ns_node_create_and_lock (namespace_t *ens, char *pathname, struct namespace_node **node, int type)
{
    namespace_internal_t *ns = (namespace_internal_t *)*ens;
    struct nsnode *n, *parent = NULL;
    struct rb_node *rbn;
    uint32_t notfound_hash;
    char *notfound_suffix = NULL, *s;
    int len, coldrun = 1;
    if( (pathname == NULL) || (node == NULL) ) {
        return ERR_ILLEGAL_ARGS;
    }
    if(strnlen(pathname, PATHNAME_MAX_BUF) == PATHNAME_MAX_BUF) {
        return ERR_PATHNAME_TOO_LONG;
    }
    if ( (*pathname == ns->separator) || (*pathname == ns->alt_separator) ) {
        pathname++;
    }
    *node = NULL;
    kobject_lock(&ns->root.lock);
    switch (local_ns_lookup(ns, pathname, &parent, &notfound_suffix, &notfound_hash)) {
    case ERR:
        // нет объекта с таким путем, можно добавлять
        break;
    case ERR_ACCESS_DENIED:
    case OK:
    default:
        // нельзя создавать вложенные объекты ядра в пустых узлах-папках, также любая недопустимая строка пути
        kobject_unlock(&ns->root.lock);
        return ERR_ACCESS_DENIED;
    }

    // сюда попали когда есть не пустой остаток пути и он свободен для добавления объекта
    // TODO возможно нужны доп. проверки на корректность пути
    pathname = notfound_suffix;
    while (HashLy_NextWord(&notfound_suffix, &notfound_hash, ns->separator, ns->alt_separator) == OK) {

        len = substrlen(pathname, ns->separator, ns->alt_separator);
        n = (struct nsnode *) kmalloc(sizeof(struct nsnode) + len + 1);
        memcpy(n->name, pathname, len);
        n->name[len] = '\0';
        kobject_lock_init_locked(&n->lock);
        n->parent = parent;
        n->prev = NULL;
        n->next = NULL;
        n->type = NS_OBJECT_FOLDER;
        n->info.ref = NULL;
        n->info.value = 0;
        if (notfound_suffix != NULL) {
            // новая папка
            n->subtree = (struct rb_tree *) kmalloc(sizeof(struct rb_tree));
            rb_tree_init(n->subtree);
        } else {
            // объект (последнее слово в пути)
            n->type = type;
            n->subtree = NULL;
            *node = &n->info;
        }
        if (coldrun != 0) {
            // первая родительская может содержать другие узлы с таким же хэш
            // или у нее может не быть поддерева так как это был лист дерева
            if(parent->subtree == NULL) {
                parent->subtree = (struct rb_tree *) kmalloc(sizeof(struct rb_tree));
                rb_tree_init(parent->subtree);
                rbn = NULL;
            } else {
                rbn = rb_tree_search(parent->subtree, notfound_hash);
            }
            coldrun = 0;
        } else {
            // последующие папки все новые
            rbn = NULL;
        }
        if (rbn != NULL) {
            // если уже есть узел у родителя с таким же хэш-значением, тогда добавляем в конец списка новый
            n->rbn = rbn;
            struct nsnode *samehash_node = (struct nsnode *) rb_node_get_data(rbn);
            samehash_node->next = n;
            n->prev = samehash_node;
            rb_node_set_data(rbn, n); // обновляем указатель на последний узел в списке
        } else {
            // выделяем память под узел дерева и вставляем его
            n->rbn = &n->local_rbn;
            rb_node_init(n->rbn);
            rb_node_set_key(n->rbn, notfound_hash);
            rb_node_set_data(n->rbn, n);
            rb_tree_insert(parent->subtree, n->rbn);
        }
        parent = n;
        pathname = notfound_suffix;
    }
    kobject_unlock_inherit(&ns->root.lock, &n->lock);
    return OK;
}

static int _ns_node_delete (namespace_internal_t *ns, struct namespace_node *node,
        int locked, int retain_while_subtree)
{
    int res;
    struct nsnode *n = (struct nsnode *) node, *parent;
    uint32_t notfound_hash;
    char *notfound_suffix, *s;

    if ((node == NULL) || (n == &ns->root)) {
        return ERR_ILLEGAL_ARGS;
    }
    // предпринимаем попытки захвата объекта почти так же как в ns_lock,
    // так как только блокированный можно удалить корректно. Разница только в том, что
    // поиск в пространстве по пути не делаем и работаем с прямой ссылкой предполагая
    // и имея в виду основное правило, которое должно соблюдаться при вызове ns_remove:
    // удалять может только модуль(процесс)-владелец ресурса с указанным путем и только исполняясь на одном из ядер
    // (от имени одного процесса)
    while (1) {
        kobject_lock(&ns->root.lock);
        if(locked) {
            break;
        }
        res = spinlock_trylock(&n->lock.slock);
        if (res == 1) {
            break;
        }
        kobject_unlock(&ns->root.lock);
        cpu_wait_energy_save(); // притормозим текущее ядро до следующего события
        // TODO расчет в любом случае на короткий период времени блокировки объекта пространства другим ядром,
        // поэтому более длительные паузы или более сложные методы синхронизации не применяем сейчас
    }

    if( (n->subtree != NULL) && (!rb_tree_is_empty(n->subtree)) ) {
        if(retain_while_subtree) {
            n->type = NS_OBJECT_DELETED;
        } else {
            n->type = NS_OBJECT_FOLDER;
            n->info.ref = NULL;
            n->info.value = 0;
        }
        if(!locked) {
            spinlock_unlock(&n->lock.slock);
        }
        kobject_unlock(&ns->root.lock);
        if(locked) {
            kobject_unlock(&n->lock);
        }
        return OK;
    }

    // начинаем с удаления узла объекта из дерева и продолжаем удалять вверх пустые папки, если они есть
    // папки считаем что не блокируются и lock в них не применяется, так как применяется глобальный лок пространства
    // для корневой папки
    do {
        parent = n->parent;
        if ((n->prev == NULL) && (n->next == NULL)) {
            rb_tree_remove(n->parent->subtree, n->rbn);
            // соответствующий узел из пространства изъят, освобождаем память
            if(n != (struct nsnode *)node) kfree(n);
        } else {
            if (n->prev != NULL) {
                n->prev->next = n->next;
            }
            if (n->next != NULL) {
                n->next->prev = n->prev;
            } else {
                // удаляли последний, обновим ссылку на последний в списке
                rb_node_set_data(n->rbn, n->prev);
            }
            // соответствующий узел из пространства изъят, освобождаем память
            if(n != (struct nsnode *)node) kfree(n);
            break;
        }
        n = parent;

    } while ((n != &ns->root) && (rb_tree_is_empty(n->subtree)));

    if(!locked) {
        spinlock_unlock(&n->lock.slock);
    }
    kobject_unlock(&ns->root.lock);
    if(locked) {
        kobject_unlock(&n->lock);
    }
    kfree(node);// память целевого узла освободили последней, так как необходима корректная разблокировка в 2-х случаях

    return OK;
}

int ns_node_delete (namespace_t *ens, struct namespace_node *node, int retain_while_subtree) {
    return _ns_node_delete((namespace_internal_t *)*ens, node, 0, retain_while_subtree);
}

int ns_node_delete_locked (namespace_t *ens, struct namespace_node *node, int retain_while_subtree) {
    return _ns_node_delete((namespace_internal_t *)*ens, node, 1, retain_while_subtree);
}

int ns_node_search_and_lock(namespace_t *ens, char *pathname, char **suffix, struct namespace_node **found)
{
    namespace_internal_t *ns = (namespace_internal_t *)*ens;
    struct nsnode *n;
    uint32_t notfound_hash;
    char *notfound_suffix;
    int res;
    int folders = 0;
    if ( (pathname == NULL) || (found == NULL) ) {
        // не задан путь и суффикс, или не задан путь и суффикс пустой, или не задан аргумент для объекта
        return ERR_ILLEGAL_ARGS;
    }
    if(strnlen(pathname, PATHNAME_MAX_BUF) == PATHNAME_MAX_BUF) {
        return ERR_PATHNAME_TOO_LONG;
    }
    if ( (*pathname == ns->separator) || (*pathname == ns->alt_separator) ) {
        pathname++;
    }
    if( (suffix != NULL) && (*suffix != NULL) ) {
        if(*suffix > (pathname + PATHNAME_MAX_BUF - 1)) {
            return ERR_ILLEGAL_ARGS;
        }
        if ( (**suffix == ns->separator) || (**suffix == ns->alt_separator) ) {
            (*suffix)++;
        }
    }

    while (1) {
        // бесконечно пытаемся сделать операцию ниже в рамках обеспечения транзакционной целостности пространства
        // по root.lock, при этом на короткое время освобождая пространство в случае неудачи (расчет на SMP режим
        // или вытеснение)
        kobject_lock(&ns->root.lock);
        n = &ns->root;

        if(suffix != NULL) {
            notfound_suffix = *suffix;
            // позиционирование на начало суффикса с пропуском всех промежуточных узлов
            res = local_ns_skip_until(ns, pathname, &n, notfound_suffix);
            if(res != OK) {
                kobject_unlock(&ns->root.lock);
                return res;
            }
        } else {
            notfound_suffix = NULL;
        }

        // каждый раз ищем узел в пространстве по всему пути, так как есть вероятность удаления узла
        // между неудачными попытками захвата
        switch (local_ns_lookup(ns, pathname, &n, &notfound_suffix, &notfound_hash)) {
        case OK:
            // объект с таким путем найден (первый встречный по pathname_suffix или точно по пути pathname)
            break;
        case ERR:
            // не найден
            kobject_unlock(&ns->root.lock);
            return ERR;
        case ERR_ACCESS_DENIED:
        default:
            kobject_unlock(&ns->root.lock);
            return ERR_ACCESS_DENIED;
        }
        if (n == &ns->root) {
            // корневой(пустой или один символ separator) нельзя лочить
            kobject_unlock(&ns->root.lock);
            return ERR_ACCESS_DENIED;
        }
        res = spinlock_trylock(&n->lock.slock);
        if (res == 1) {
            kobject_unlock_inherit(&ns->root.lock, &n->lock);
            *found = (struct namespace_node *)n;
            if(suffix != NULL) {
                *suffix = notfound_suffix;
            }
            break;
        }
        kobject_unlock(&ns->root.lock);
        cpu_wait_energy_save(); // притормозим текущее ядро до следующего события
        // TODO расчет в любом случае на короткий период времени блокировки объекта пространства другим ядром,
        // поэтому более длительные паузы или более сложные методы синхронизации не применяем сейчас
    }
    return OK;
}

int ns_node_unlock (namespace_t *ens, struct namespace_node *node)
{
//    namespace_internal_t *ns = (namespace_internal_t *)*ens;
    struct nsnode *n = (struct nsnode *) node;
    int res;
    if (node == NULL) {
        return ERR_ILLEGAL_ARGS;
    }
//    spinlock_unlock(&n->lock.slock);
    kobject_unlock(&n->lock);
    return OK;
}

char ns_get_separator(namespace_t *ens) {
    return ((namespace_internal_t *)*ens)->separator;
}

char ns_get_alt_separator(namespace_t *ens) {
    return ((namespace_internal_t *)*ens)->alt_separator;
}

int ns_node_get_objtype(struct namespace_node *node) {
    struct nsnode *n = (struct nsnode *) node;
    return n->type;
}
