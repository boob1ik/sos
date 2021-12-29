#include <rbtree.h>

// @TODO 1) Необходимо оптимизировать методы rb_tree_get_min, rb_tree_get_max
// в части постоянного отслеживания минимального и максимального элемента по значению ключа
// при изменении дерева при добавлении и удалении элементов. Это даст
// возможность сразу получать минимальный и максимальный элемент по ссылке без поиска.

// @TODO 2) Необходимо ввести глобальную ссылку-переменную как результат любой из операций
// поиска для эффективного возможного вызова метода вставки или удаления этого же элемента
// или смежного в этой же ветке дерева. Это позволит после вызова search выполнять вставку
// не найденного элемента или удаление найденного элемента последущим вызовом без поиска
// соответствующей позиции, то есть без повторного прохода с вершины дерева

static struct rb_node rbnull = {
        { .up_color = ((&rbnull) + RBNODE_COLOR_BLACK) }, .left = &rbnull,
        .right = &rbnull, .key = 0, .data = NULL };
static struct rb_node *NULL_NODE = &rbnull;

static inline void set_up (struct rb_node *node, struct rb_node *ptr)
{
    if (!node)
        return;
    node->up = ((uint32_t) ptr) >> 2;
}
static inline struct rb_node* get_up (struct rb_node *node)
{
    if (!node)
        return NULL;
    return (struct rb_node*) ((node->up) << 2);
}

void rb_tree_init (struct rb_tree *tree)
{
    if (!tree)
        return;
    tree->root = NULL_NODE;
    tree->tree_mode = RBTREE_BY_KEY_VALUE;
    tree->nodes = 0;
    tree->stat.inserts = 0;
    tree->stat.removes = 0;
    tree->stat.rotations = 0;
}

void rb_node_init (struct rb_node *node)
{
    if (!node)
        return;
    node->up_color = 0;
    node->left = node->right = NULL_NODE;
}

void rb_node_set_key (struct rb_node *node, size_t key)
{
    if (!node)
        return;
    node->key = key;
}

size_t rb_node_get_key (struct rb_node *node)
{
    if (!node)
        return 0;
    return node->key;
}

void rb_node_set_data (struct rb_node *node, void *data)
{
    if (!node)
        return;
    node->data = data;
}

void* rb_node_get_data (struct rb_node *node)
{
    if (!node)
        return NULL;
    return node->data;
}

void rb_tree_set_mode(struct rb_tree *tree, enum rb_tree_mode mode) {
    if (!tree)
        return;
    tree->tree_mode = mode;
}

static void rotate_left (struct rb_tree *tree, struct rb_node *x)
{
    // requirement: x != NULL, x->right != NULL_NODE, root->up == NULL_NODE
    struct rb_node *y = x->right, *p = get_up(x);
    x->right = y->left;
    if (y->left != NULL_NODE)
        set_up(y->left, x);
    set_up(y, p);
    if (p == NULL_NODE) {
        set_up(y, NULL_NODE);
        tree->root = y;
    } else if (x == p->left) {
        p->left = y;
    } else {
        p->right = y;
    }
    y->left = x;
    set_up(x, y);

    tree->stat.rotations++;
}

static void rotate_right (struct rb_tree *tree, struct rb_node *x)
{
    // requirement: x != NULL, x->left != NULL_NODE, root->up == NULL_NODE
    struct rb_node *y = x->left, *p = get_up(x);
    x->left = y->right;
    if (y->right != NULL_NODE)
        set_up(y->right, x);
    set_up(y, p);
    if (p == NULL_NODE) {
        set_up(y, NULL_NODE);
        tree->root = y;
    } else if (x == p->right) {
        p->right = y;
    } else {
        p->left = y;
    }
    y->right = x;
    set_up(x, y);

    tree->stat.rotations++;
}

static void insert_balance (struct rb_tree *tree, struct rb_node *x)
{
    struct rb_node *p1 = get_up(x), *p2 = get_up(p1);

    while ((p2 != NULL_NODE) && (p1->color == RBNODE_COLOR_RED)) {
        if (p1 == (p2->left)) {
            struct rb_node *y = p2->right;
            if (y->color == RBNODE_COLOR_RED) {
                p1->color = RBNODE_COLOR_BLACK;
                y->color = RBNODE_COLOR_BLACK;
                p2->color = RBNODE_COLOR_RED;
                x = p2;
            } else {
                if (x == p1->right) {
                    x = p1;
                    rotate_left(tree, x);
                    p1 = get_up(x);
                    p2 = get_up(p1);
                }
                p1->color = RBNODE_COLOR_BLACK;
                p2->color = RBNODE_COLOR_RED;
                rotate_right(tree, p2);
            }
        } else {
            struct rb_node *y = p2->left;
            if (y->color == RBNODE_COLOR_RED) {
                p1->color = RBNODE_COLOR_BLACK;
                y->color = RBNODE_COLOR_BLACK;
                p2->color = RBNODE_COLOR_RED;
                x = p2;
            } else {
                if (x == p1->left) {
                    x = p1;
                    rotate_right(tree, x);
                    p1 = get_up(x);
                    p2 = get_up(p1);
                }
                p1->color = RBNODE_COLOR_BLACK;
                p2->color = RBNODE_COLOR_RED;
                rotate_left(tree, p2);
            }
        }
        p1 = get_up(x);
        p2 = get_up(p1);
    }
    tree->root->color = RBNODE_COLOR_BLACK;
}

static void remove_balance (struct rb_tree *tree, struct rb_node *x)
{
    struct rb_node *p = get_up(x);

    while ( (p != NULL_NODE) && (x->color == RBNODE_COLOR_BLACK)) {
        if (x == p->left) {
            struct rb_node *y = p->right;
            if (y->color == RBNODE_COLOR_RED) {
                y->color = RBNODE_COLOR_BLACK;
                p->color = RBNODE_COLOR_RED;
                rotate_left(tree, p);
                p = get_up(x);
                y = p->right;
            }
            if ((y->left->color == RBNODE_COLOR_BLACK)
                    && (y->right->color == RBNODE_COLOR_BLACK)) {
                y->color = RBNODE_COLOR_RED;
                x = p;
            } else {
                if (y->right->color == RBNODE_COLOR_BLACK) {
                    y->left->color = RBNODE_COLOR_BLACK;
                    y->color = RBNODE_COLOR_RED;
                    rotate_right(tree, y);
                    p = get_up(x);
                    y = p->right;
                }
                y->color = p->color;
                p->color = RBNODE_COLOR_BLACK;
                y->right->color = RBNODE_COLOR_BLACK;
                rotate_left(tree, p);
                x = tree->root;
            }
        } else {
            struct rb_node *y = p->left;
            if (y->color == RBNODE_COLOR_RED) {
                y->color = RBNODE_COLOR_BLACK;
                p->color = RBNODE_COLOR_RED;
                rotate_right(tree, p);
                p = get_up(x);
                y = p->left;
            }
            if ((y->right->color == RBNODE_COLOR_BLACK)
                    && (y->left->color == RBNODE_COLOR_BLACK)) {
                y->color = RBNODE_COLOR_RED;
                x = p;
            } else {
                if (y->left->color == RBNODE_COLOR_BLACK) {
                    y->right->color = RBNODE_COLOR_BLACK;
                    y->color = RBNODE_COLOR_RED;
                    rotate_left(tree, y);
                    p = get_up(x);
                    y = p->left;
                }
                y->color = p->color;
                p->color = RBNODE_COLOR_BLACK;
                y->left->color = RBNODE_COLOR_BLACK;
                rotate_right(tree, p);
                x = tree->root;
            }
        }
        p = get_up(x);
    }
    x->color = RBNODE_COLOR_BLACK;
}

void rb_tree_insert (struct rb_tree *tree, struct rb_node *n)
{
    if (!n || !tree)
        return;

    struct rb_node *p = NULL_NODE, *y = tree->root;

    n->left = NULL_NODE;
    n->right = NULL_NODE;
    if ((y == NULL) || (y == NULL_NODE)) {
        tree->root = n;
        set_up(n, NULL_NODE);
        n->color = RBNODE_COLOR_BLACK;
        tree->nodes = 1;
        tree->stat.inserts++;
        return;
    }

    switch (tree->tree_mode) {
    case RBTREE_BY_KEY_VALUE:
        while (y != NULL_NODE) {
            p = y;
            if (n->key < y->key)
                y = y->left;
            else
                y = y->right;
        }
        break;
    case RBTREE_BY_PTR_ADDRESS:
        while (y != NULL_NODE) {
            p = y;
            if (((uint32_t) &(n->data)) < ((uint32_t) &(y->data)))
                y = y->left;
            else
                y = y->right;
        }
        break;
    }

    n->color = RBNODE_COLOR_RED;
    set_up(n, p);

    switch (tree->tree_mode) {
    case RBTREE_BY_KEY_VALUE:
        if (n->key < p->key) {
            p->left = n;
        } else {
            p->right = n;
        }
        break;
    case RBTREE_BY_PTR_ADDRESS:
        if (((uint32_t) &(n->data)) < ((uint32_t) &(p->data))) {
            p->left = n;
        } else {
            p->right = n;
        }
        break;
    }
    insert_balance(tree, n);
    tree->nodes++;
    tree->stat.inserts++;
}

void rb_tree_remove (struct rb_tree *tree, struct rb_node *z)
{
    struct rb_node *y, *x;
    struct rb_node *p;
    enum rb_node_color c;
    if (!tree || !z)
        return;

    if ((z->left == NULL_NODE) || (z->right == NULL_NODE)) {
        y = z;
    } else {
        y = z->right;
        while (y->left != NULL_NODE)
            y = y->left;
    }

    x = (y->left != NULL_NODE) ? y->left : y->right;

    p = get_up(y);
    set_up(x, p);
    if (p != NULL_NODE) {
        if (y == p->left) {
            p->left = x;
        } else {
            p->right = x;
        }
    } else
        tree->root = x;

    c = y->color;
    if (y != z) {
        // перемещаем ! узел вместо удаляемого
        p = get_up(z);
        set_up(y, p);
        if (p == NULL_NODE) {
            tree->root = y;
        } else if(p->left == z) {
            p->left = y;
        } else {
            p->right = y;
        }
        if(z->left != y) {
            y->left = z->left;
            set_up(y->left, y);
        } else {
            y->left = NULL_NODE;
        }
        if(z->right != y) {
            y->right = z->right;
            set_up(y->right, y);
        } else {
            y->right = NULL_NODE;
        }
        y->color = z->color;
    }
    if ( c == RBNODE_COLOR_BLACK )
        remove_balance(tree, x);
    tree->nodes--;
    tree->stat.removes++;
}

struct rb_node* rb_tree_search (struct rb_tree *tree, size_t key)
{
    uint32_t val;
    if (!tree)
        return NULL;

    struct rb_node *node = tree->root;

    if (!node || (node == NULL_NODE))
        return NULL;

    switch (tree->tree_mode) {
    case RBTREE_BY_KEY_VALUE:
        while (node != NULL_NODE) {
            val = node->key;
            if (val == key)
                return node;
            else
                node = key < val ? node->left : node->right;
        }
        break;
    case RBTREE_BY_PTR_ADDRESS:
        while (node != NULL_NODE) {
            val = (uint32_t) &(node->data);
            if (val == key)
                return node;
            else
                node = key < val ? node->left : node->right;
        }
        break;
    }
    return NULL;
}

static struct rb_node* rb_tree_search_neareqless_loc (struct rb_tree *tree,
        size_t key, int equal)
{
    if (!tree)
        return NULL;

    size_t val1, val2;
    struct rb_node *node = tree->root;
    struct rb_node *found = NULL;

    if (!node || (node == NULL_NODE))
        return found;

    switch (tree->tree_mode) {
    case RBTREE_BY_KEY_VALUE:
        while (node != NULL_NODE) {
            val1 = node->key;
            if (val1 == key) {
                if (equal)
                    return node;
                else {
                    if (node->left != NULL_NODE) {
                        node = node->left;
                    } else {
                        return found;
                    }
                }
            } else if (val1 < key) {
                found = node;
//                if (node->right == NULL_NODE)
//                    return found;
//                val2 = node->right->key;
//                if (val2 > key)
//                    return found;
                node = node->right;
            } else {
                node = node->left;
            }
        }
        break;
    case RBTREE_BY_PTR_ADDRESS:
        while (node != NULL_NODE) {
            val1 = (size_t) &(node->data);
            if (val1 == key) {
                if (equal)
                    return node;
                else {
                    if (node->left != NULL_NODE) {
                        node = node->left;
                    } else {
                        return found;
                    }
                }
            } else if (val1 < key) {
                found = node;
//                if (node->right == NULL_NODE)
//                    return found;
//                val2 = (size_t) &(node->right->data);
//                if (val2 > key)
//                    return found;
                node = node->right;
            } else {
                node = node->left;
            }
        }
        break;
    }
    return found;
}

static struct rb_node* rb_tree_search_neareqlarger_loc (struct rb_tree *tree,
        size_t key, int equal)
{
    if (!tree)
        return NULL;

    size_t val1, val2;
    struct rb_node *node = tree->root;
    struct rb_node *found = NULL;

    if (!node || (node == NULL_NODE))
        return found;

    switch (tree->tree_mode) {
    case RBTREE_BY_KEY_VALUE:
        while (node != NULL_NODE) {
            val1 = node->key;
            if (val1 == key) {
                if (equal)
                    return node;
                else {
                    if (node->right != NULL_NODE) {
                        node = node->right;
                    } else {
                        return found;
                    }
                }
            } else if (val1 > key) {
                found = node;
//                if (node->left == NULL_NODE)
//                    return found;
//                val2 = node->left->key;
//                if (val2 < key)
//                    return found;
                node = node->left;
            } else {
                node = node->right;
            }
        }
        break;
    case RBTREE_BY_PTR_ADDRESS:
        while (node != NULL_NODE) {
            val1 = (size_t) &(node->data);
            if (val1 == key) {
                if (equal)
                    return node;
                else {
                    if (node->right != NULL_NODE) {
                        node = node->right;
                    } else {
                        return found;
                    }
                }
            } else if (val1 > key) {
                found = node;
//                if (node->left == NULL_NODE)
//                    return found;
//                val2 = (size_t) &(node->left->data);
//                if (val2 < key)
//                    return found;
                node = node->left;
            } else {
                node = node->right;
            }
        }
        break;
    }
    return found;
}

static struct rb_node* rb_tree_search_eqless_loc (struct rb_tree *tree,
        size_t key, int equal)
{
    if (!tree)
        return NULL;

    size_t val1;
    struct rb_node *node = tree->root;
    struct rb_node *found = NULL;

    if (!node || (node == NULL_NODE))
        return found;

    switch (tree->tree_mode) {
    case RBTREE_BY_KEY_VALUE:
        while (node != NULL_NODE) {
            val1 = node->key;
            if (val1 == key) {
                if (equal)
                    return node;
                else {
                    if (node->left != NULL_NODE) {
                        node = node->left;
                    } else {
                        return found;
                    }
                }
            } else if (val1 < key) {
                return node;
            } else {
                node = node->left;
            }
        }
        break;
    case RBTREE_BY_PTR_ADDRESS:
        while (node != NULL_NODE) {
            val1 = (size_t) &(node->data);
            if (val1 == key) {
                if (equal)
                    return node;
                else {
                    if (node->left != NULL_NODE) {
                        node = node->left;
                    } else {
                        return found;
                    }
                }
            } else if (val1 < key) {
                return node;
            } else {
                node = node->left;
            }
        }
        break;
    }
    return found;
}

static struct rb_node* rb_tree_search_eqlarger_loc (struct rb_tree *tree,
        size_t key, int equal)
{
    if (!tree)
        return NULL;

    size_t val1;
    struct rb_node *node = tree->root;
    struct rb_node *found = NULL;

    if (!node || (node == NULL_NODE))
        return found;

    switch (tree->tree_mode) {
    case RBTREE_BY_KEY_VALUE:
        while (node != NULL_NODE) {
            val1 = node->key;
            if (val1 == key) {
                if (equal)
                    return node;
                else {
                    if (node->right != NULL_NODE) {
                        node = node->right;
                    } else {
                        return found;
                    }
                }
            } else if (val1 > key) {
                return node;
            } else {
                node = node->right;
            }
        }
        break;
    case RBTREE_BY_PTR_ADDRESS:
        while (node != NULL_NODE) {
            val1 = (size_t) &(node->data);
            if (val1 == key) {
                if (equal)
                    return node;
                else {
                    if (node->right != NULL_NODE) {
                        node = node->right;
                    } else {
                        return found;
                    }
                }
            } else if (val1 > key) {
                return node;
            } else {
                node = node->right;
            }
        }
        break;
    }
    return found;
}


struct rb_node* rb_tree_search_nearless (struct rb_tree *tree, size_t key)
{
    return rb_tree_search_neareqless_loc(tree, key, 0);
}

struct rb_node* rb_tree_search_nearlarger (struct rb_tree *tree, size_t key)
{
    return rb_tree_search_neareqlarger_loc(tree, key, 0);
}

struct rb_node* rb_tree_search_neareqless (struct rb_tree *tree, size_t key)
{
    return rb_tree_search_neareqless_loc(tree, key, 1);
}

struct rb_node* rb_tree_search_neareqlarger (struct rb_tree *tree,
        size_t key)
{
    return rb_tree_search_neareqlarger_loc(tree, key, 1);
}

struct rb_node* rb_tree_search_less (struct rb_tree *tree, size_t key)
{
    return rb_tree_search_eqless_loc(tree, key, 0);
}

struct rb_node* rb_tree_search_larger (struct rb_tree *tree, size_t key)
{
    return rb_tree_search_eqlarger_loc(tree, key, 0);
}

struct rb_node* rb_tree_search_eqless (struct rb_tree *tree, size_t key)
{
    return rb_tree_search_eqless_loc(tree, key, 1);
}

struct rb_node* rb_tree_search_eqlarger (struct rb_tree *tree,
        size_t key)
{
    return rb_tree_search_eqlarger_loc(tree, key, 1);
}

struct rb_node* rb_tree_get_nearlarger (struct rb_node *node)
{
    if (!node)
        return NULL;
    struct rb_node *y = node, *u;
    if (y->right == NULL_NODE) {
        u = get_up(y);
        while ((u != NULL_NODE) && (u->right == y)) {
            y = u;
            u = get_up(y);
        }
        y = u;
    } else {
        y = y->right;
        while (y->left != NULL_NODE)
            y = y->left;
    }
    if ((y == node) || (y == NULL_NODE))
        y = NULL;
    return y;
}

struct rb_node* rb_tree_get_nearless (struct rb_node *node)
{
    if (!node)
        return NULL;
    struct rb_node *y = node, *u = get_up(y);
    if (y->left == NULL_NODE) {
        u = get_up(y);
        while ((u != NULL_NODE) && (u->left == y)) {
            y = u;
            u = get_up(y);
        }
        y = u;
    } else {
        y = y->left;
        while (y->right != NULL_NODE)
            y = y->right;
    }
    if ((y == node) || (y == NULL_NODE))
        y = NULL;
    return y;
}

struct rb_node* rb_tree_get_min (struct rb_tree *tree) {
    if (!tree)
        return NULL;
    struct rb_node *y = tree->root;
    if(y != NULL_NODE) {
        while(y->left != NULL_NODE) {
            y = y->left;
        }
    } else {
        y = NULL;
    }
    return y;
}

struct rb_node* rb_tree_get_max (struct rb_tree *tree) {
    if (!tree)
        return NULL;
    struct rb_node *y = tree->root;
    if(y != NULL_NODE) {
        while(y->right != NULL_NODE) {
            y = y->right;
        }
    } else {
        y = NULL;
    }
    return y;
}

struct rb_node* rb_tree_get_root (struct rb_tree *tree) {
    return tree->root;
}

extern int rb_tree_is_empty (struct rb_tree *tree) {
    return (tree->root == NULL_NODE);
}

int rb_tree_get_next (struct rb_node *node, struct rb_node **next) {
    int res = 1;
    if (!node)
        return 0;
    struct rb_node *y = node, *u;
    if (y->right == NULL_NODE) {
        res = 0;
        u = get_up(y);
        while ((u != NULL_NODE) && (u->right == y)) {
            y = u;
            u = get_up(y);
        }
        y = u;
    } else {
        y = y->right;
        while (y->left != NULL_NODE)
            y = y->left;
    }
    if ((y == node) || (y == NULL_NODE))
        y = NULL;

    if(next != NULL) {
        *next = y;
    }
    return res;
}

static int rb_tree_clear_recursive(struct rb_tree *tree, struct rb_node *node,
        void (*node_free)(struct rb_node *node)) {
    int res = 0;
    if( (node == NULL) || (node == NULL_NODE)) {
        return res;
    }
    res += rb_tree_clear_recursive(tree, node->left, node_free);
    res += rb_tree_clear_recursive(tree, node->right, node_free);
    if(node_free != NULL) {
        node_free(node);
    }
    res++;
    return res;
}

int rb_tree_free (struct rb_tree *tree, void (*node_free)(struct rb_node *node)) {
    if(rb_tree_clear_recursive(tree, tree->root, node_free) == tree->nodes) {
        tree->nodes = 0;
        tree->stat.inserts = 0;
        tree->stat.removes = 0;
        tree->stat.rotations = 0;
        return 1;
    } else return 0;
}

size_t rb_tree_get_nodes_count(struct rb_tree *tree) {
    return tree->nodes;
}
