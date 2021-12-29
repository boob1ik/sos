#ifndef RBTREE4HEAP_H_
#define RBTREE4HEAP_H_
#include <rbtree.h>

/**
 * Красно-черное дерево свободных блоков памяти по размеру, является
 * специализированным деревом с двусвязным списком по одинаковым размерам.
 * Сортировка только по значению ключа - размеру блока памяти.
 * Свободные блоки одновременно являются частью двух деревьев rb_tree и rb_tree4heap
 * с сортировкой по адресам и по размеру, поле key является общим.
 * В основном красно-черном дереве участвуют занятые и свободные блоки памяти,
 * сортированные по адресу начала пользовательских данных.
 * Специализированное дерево по размеру строится на базе дополнительного комплекта
 * служебных полей и указателей с сортировкой по полю key(по полному размеру блока
 * включая служебную информацию). Так как блоков одного размера может быть много, то к
 * каждому из блоков, участвующих в дереве, может быть привязан двусвязный список
 * блоков такого же размера. Первый блок является корневым двусвязного списка и является частью
 * дерева. Указатель dlist показывает на первый блок в двусвязном списке. Блоки
 * из двусвязного списка не участвуют в специализированном дереве напрямую (участвуют
 * только в дереве по адресам), указатели rb_node4heap.left, right в них работают для организации
 * списка, up - для возврата к корневому блоку такого же размера но уже являющегося частью дерева
 * В то же время все типовые операции с элементами дерева также работают с элементами
 * двусвязного списка. При вставке элемента в дереве определяется, будет ли он корневым для списка
 * данного размера в дереве или будет входить в список по признаку rb_node4heap.user_bit
 */
struct rb_node4heap {
    size_t unused[3]; // скрытые поля rb_node дерева rb_tree
    size_t key;
    union {
        struct rb_node4heap *up_color;
        struct {
            enum rb_node_color color :1;
            size_t user_bit :1; // 1 -используется как признак узла,
            // участвующего в двусвязном списке свободных узлов одного размера
            size_t up :30; // для двусвязного списка - корневой узел того же размера,
            // который является частью дерева
        };
    };
    struct rb_node4heap *left;  // для двусвязного списка - левый в списке
    struct rb_node4heap *right; // для двусвязного списка - правый в списке
    struct rb_node4heap *dlist; // если не пустой - указывает на первый элемент двусвязного списка
};

#define ALLOCATED_CHUNK_HDR_SIZE    (sizeof(size_t) * 4)
#define MIN_CHUNK_SIZE              (sizeof(struct rb_node4heap))
#define MIN_CHUNK_USER_SIZE         (MIN_CHUNK_SIZE - ALLOCATED_CHUNK_HDR_SIZE)

struct rb_tree4heap {
    struct rb_node4heap *root;          //!< вершина дерева
    size_t nodes;                     //!< число узлов в дереве
};


extern void hrb_tree_init (struct rb_tree4heap *tree);
extern void hrb_node_init (struct rb_node4heap *node);

extern void hrb_tree_clear (struct rb_tree4heap *tree);
extern void hrb_tree_insert (struct rb_tree4heap *tree, struct rb_node4heap *node);
extern void hrb_tree_remove (struct rb_tree4heap *tree, struct rb_node4heap *node);

extern struct rb_node4heap* hrb_tree_search_neareqlarger (struct rb_tree4heap *tree, size_t key);
extern struct rb_node4heap* hrb_tree_search_eqlarger (struct rb_tree4heap *tree, size_t key);
extern struct rb_node4heap* hrb_tree_get_neareqlarger (struct rb_node4heap *listroot,
        struct rb_node4heap *node);
#endif /* RBTREE4HEAP_H_ */
