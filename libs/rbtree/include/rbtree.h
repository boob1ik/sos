/** \brief red-black tree */

#ifndef RBTREE_H
#ifdef __cplusplus
extern "C" {
#endif
#define RBTREE_H

#include <stdint.h>
#include <stddef.h>

enum rb_node_color {
    RBNODE_COLOR_RED,
    RBNODE_COLOR_BLACK
};

enum rb_tree_mode {
    RBTREE_BY_KEY_VALUE,
    RBTREE_BY_PTR_ADDRESS
};

struct rb_node {
    union {
        struct rb_node *up_color;
        struct {
            enum rb_node_color color :1;
            size_t user_bit :1;
            size_t up :30;
        };
    };
    struct rb_node *left;
    struct rb_node *right;
    size_t key;
    void *data;
};

struct rb_tree {
    struct rb_node *root;           //!< вершина дерева
    enum rb_tree_mode tree_mode;    //!< ключ дерева
    size_t nodes;                   //!< число узлов в дереве
    struct {
        volatile uint64_t inserts;
        volatile uint64_t removes;
        volatile uint64_t rotations;
    } stat;
};

extern void rb_tree_init (struct rb_tree *tree);
extern void rb_node_init (struct rb_node *node);

extern void rb_tree_set_mode(struct rb_tree *tree, enum rb_tree_mode mode);

extern void rb_node_set_key (struct rb_node *node, size_t key);
extern size_t rb_node_get_key (struct rb_node *node);

extern void rb_node_set_data (struct rb_node *node, void *data);
extern void* rb_node_get_data (struct rb_node *node);

extern int rb_tree_free (struct rb_tree *tree, void (*node_free)(struct rb_node *node));
extern void rb_tree_insert (struct rb_tree *tree, struct rb_node *node);
extern void rb_tree_remove (struct rb_tree *tree, struct rb_node *node);

extern struct rb_node* rb_tree_search (struct rb_tree *tree, size_t key);
extern struct rb_node* rb_tree_search_nearlarger (struct rb_tree *tree, size_t key);
extern struct rb_node* rb_tree_search_nearless (struct rb_tree *tree, size_t key);
extern struct rb_node* rb_tree_search_neareqlarger (struct rb_tree *tree, size_t key);
extern struct rb_node* rb_tree_search_neareqless (struct rb_tree *tree, size_t key);

extern struct rb_node* rb_tree_search_larger (struct rb_tree *tree, size_t key);
extern struct rb_node* rb_tree_search_less (struct rb_tree *tree, size_t key);
extern struct rb_node* rb_tree_search_eqlarger (struct rb_tree *tree, size_t key);
extern struct rb_node* rb_tree_search_eqless (struct rb_tree *tree, size_t key);

extern struct rb_node* rb_tree_get_nearlarger (struct rb_node *node);
extern struct rb_node* rb_tree_get_nearless (struct rb_node *node);

extern struct rb_node* rb_tree_get_root (struct rb_tree *tree);
extern int rb_tree_is_empty (struct rb_tree *tree);
extern size_t rb_tree_get_nodes_count(struct rb_tree *tree);
extern int rb_tree_get_next (struct rb_node *node, struct rb_node **next);

extern struct rb_node* rb_tree_get_min (struct rb_tree *tree);
extern struct rb_node* rb_tree_get_max (struct rb_tree *tree);

#ifdef __cplusplus
}
#endif
#endif 
