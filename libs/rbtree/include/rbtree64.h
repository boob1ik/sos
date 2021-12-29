#ifndef RBTREE64_H_
/** \brief red-black tree with 64-bit key */

#ifdef __cplusplus
extern "C" {
#endif
#define RBTREE64_H_

#include "rbtree.h"

struct rb_node64 {
    uint64_t key;
    union {
        struct rb_node64 *up_color;
        struct {
            enum rb_node_color color :1;
            size_t user_bit :1;
            size_t up :30;
        };
    };
    struct rb_node64 *left;
    struct rb_node64 *right;
    void *data;
};

struct rb_tree64 {
    struct rb_node64 *root;           //!< вершина дерева
    enum rb_tree_mode tree_mode;    //!< ключ дерева
    size_t nodes;                   //!< число узлов в дереве
};

extern void rb_tree64_init (struct rb_tree64 *tree);
extern void rb_node64_init (struct rb_node64 *node);

extern void rb_tree64_set_mode(struct rb_tree64 *tree, enum rb_tree_mode mode);

extern void rb_node64_set_key (struct rb_node64 *node, uint64_t key);
extern uint64_t rb_node64_get_key (struct rb_node64 *node);

extern void rb_node64_set_data (struct rb_node64 *node, void *data);
extern void* rb_node64_get_data (struct rb_node64 *node);

extern void rb_tree64_clear (struct rb_tree64 *tree);
extern void rb_tree64_insert (struct rb_tree64 *tree, struct rb_node64 *node);
extern void rb_tree64_remove (struct rb_tree64 *tree, struct rb_node64 *node);

extern struct rb_node64* rb_tree64_search (struct rb_tree64 *tree, uint64_t key);
extern struct rb_node64* rb_tree64_search_nearlarger (struct rb_tree64 *tree, uint64_t key);
extern struct rb_node64* rb_tree64_search_nearless (struct rb_tree64 *tree, uint64_t key);
extern struct rb_node64* rb_tree64_search_neareqlarger (struct rb_tree64 *tree, uint64_t key);
extern struct rb_node64* rb_tree64_search_neareqless (struct rb_tree64 *tree, uint64_t key);

extern struct rb_node64* rb_tree64_search_larger (struct rb_tree64 *tree, uint64_t key);
extern struct rb_node64* rb_tree64_search_less (struct rb_tree64 *tree, uint64_t key);
extern struct rb_node64* rb_tree64_search_eqlarger (struct rb_tree64 *tree, uint64_t key);
extern struct rb_node64* rb_tree64_search_eqless (struct rb_tree64 *tree, uint64_t key);

extern struct rb_node64* rb_tree64_get_nearlarger (struct rb_node64 *node);
extern struct rb_node64* rb_tree64_get_nearless (struct rb_node64 *node);

extern struct rb_node64* rb_tree64_get_min (struct rb_tree64 *tree);
extern struct rb_node64* rb_tree64_get_max (struct rb_tree64 *tree);

#ifdef __cplusplus
}
#endif


#endif /* RBTREE64_H_ */
