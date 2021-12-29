#include "rbtree4heap.h"

static struct rb_node4heap rbnull = { { 0, 0, 0 }, 0, { .up_color = ((&rbnull)
        + RBNODE_COLOR_BLACK) }, .left = &rbnull, .right = &rbnull, .key = 0,
        .dlist = NULL };
static struct rb_node4heap *NULL_NODE = &rbnull;

static inline void set_up (struct rb_node4heap *node, struct rb_node4heap *ptr)
{
    if (!node)
        return;
    node->up = ((size_t) ptr) >> 2;
}
static inline struct rb_node4heap* get_up (struct rb_node4heap *node)
{
    if (!node)
        return NULL;
    return (struct rb_node4heap*) ((node->up) << 2);
}

void hrb_node_init (struct rb_node4heap *node)
{
    if (!node)
        return;
    node->up_color = 0;
    node->left = node->right = NULL_NODE;
    node->dlist = NULL;
}

void hrb_tree_init (struct rb_tree4heap *tree)
{
    tree->root = NULL_NODE;
    tree->nodes = 0;
}

static void rotate_left (struct rb_tree4heap *tree, struct rb_node4heap *x)
{
    // requirement: x != NULL, x->right != NULL_NODE, root->up == NULL_NODE
    struct rb_node4heap *y = x->right, *p = get_up(x);
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
}

static void rotate_right (struct rb_tree4heap *tree, struct rb_node4heap *x)
{
    // requirement: x != NULL, x->left != NULL_NODE, root->up == NULL_NODE
    struct rb_node4heap *y = x->left, *p = get_up(x);
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
}

static void insert_balance (struct rb_tree4heap *tree, struct rb_node4heap *x)
{
    struct rb_node4heap *p1 = get_up(x), *p2 = get_up(p1);

    while ((p2 != NULL_NODE) && (p1->color == RBNODE_COLOR_RED)) {
        if (p1 == (p2->left)) {
            struct rb_node4heap *y = p2->right;
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
            struct rb_node4heap *y = p2->left;
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

static void remove_balance (struct rb_tree4heap *tree, struct rb_node4heap *x)
{
    struct rb_node4heap *p = get_up(x);

    while ((p != NULL_NODE) && (x->color == RBNODE_COLOR_BLACK)) {
        if (x == p->left) {
            struct rb_node4heap *y = p->right;
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
            struct rb_node4heap *y = p->left;
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

void hrb_tree_insert (struct rb_tree4heap *tree, struct rb_node4heap *n)
{
    if (!n || !tree)
        return;

    struct rb_node4heap *p = NULL_NODE, *y = tree->root;

    if ((y == NULL) || (y == NULL_NODE)) {
        tree->root = n;
        set_up(n, NULL_NODE);
        n->color = RBNODE_COLOR_BLACK;
        tree->nodes = 1;
        return;
    }

    while (y != NULL_NODE) {
        p = y;
        if (n->key < y->key)
            y = y->left;
        else if (y->key == n->key) {
            n->left = NULL;
            // обработка специального случая,
            // когда уже есть элемент в дереве с таким же ключом
            // балансировка потом не нужна
            if (y->dlist != NULL) {
                // вставляем всегда в начало списка
                y->dlist->left = n;
                n->right = y->dlist;
            } else {
                n->right = NULL;
            }
            y->dlist = n;
            set_up(n, y);
            n->user_bit = 1;
            tree->nodes++;
            return;
        } else
            y = y->right;
    }

    n->color = RBNODE_COLOR_RED;
    set_up(n, p);

    if (n->key < p->key) {
        p->left = n;
    } else {
        p->right = n;
    }
    insert_balance(tree, n);
    tree->nodes++;
}

void hrb_tree_remove (struct rb_tree4heap *tree, struct rb_node4heap *z)
{
    struct rb_node4heap *y, *x;
    struct rb_node4heap *p;
    enum rb_node_color c;
    if (!z)
        return;

    // специальная обработка узла, который относится к двусвязному списку и, возможно, является
    // последним элементом в списке (но не корневым для этого списка того же размера,
    // корневой является обычным элементом дерева)
    if (z->user_bit) {
        z->user_bit = 0;
        tree->nodes--;
        if (!z->left && !z->right) {
            // последний в списке, просто обнуляем указатель в корневом элементе списка
            get_up(z)->dlist = NULL;
            return;
        }
        if (!z->left) {
            p = get_up(z);
            // первый но не последний, обновляем указатель в корневом элементе списка
            p->dlist = z->right;
            z->right->left = NULL;
            // Всегда поддерживаем в первом элементе списка корректную ссылку наверх
            // на корневой элемент списка, в остальных элементах эта ссылка может
            // стать неверной если удаляется корневой элемент списка
            set_up(z->right, p);
            return;
        } else {
            z->left->right = z->right;
        }
        if (z->right) {
            z->right->left = z->left;
        }
        return;
    } else if (z->dlist != NULL) {
        tree->nodes--;
        // пытаемся удалить корневой элемент списка из дерева,
        // берем первый в списке и делаем корневым для списка вместо исходного
        y = z->dlist;
        y->user_bit = 0;
        y->dlist = y->right;
        if (y->dlist) {
            y->dlist->left = NULL;
            set_up(y->dlist, y); // не забываем обновить ссылку в первом элементе на новый корневой
        }
        // обновляем ссылки соседей в дереве и нового элемента
        // up
        p = get_up(z);
        if (p != NULL_NODE) {
            if (p->left == z) {
                p->left = y;
            } else {
                p->right = y;
            }
        } else {
            // удаляемый был корневой дерева, обновляем также ссылку на корень дерева
            tree->root = y;
        }
        set_up(y, p);
        // left
        if (z->left != NULL_NODE) {
            set_up(z->left, y);
        }
        y->left = z->left;
        // right
        if (z->right != NULL_NODE) {
            set_up(z->right, y);
        }
        y->right = z->right;
        y->color = z->color;
        y->user_bit = 0;
        return;
    }
    // иначе обычный алгоритм удаления

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
        if (y == p->left)
            p->left = x;
        else
            p->right = x;
    } else
        tree->root = x;

    c = y->color;
    if (y != z) {
        // перемещаем ! узел вместо удаляемого
        p = get_up(z);
        set_up(y, p);
        if (p == NULL_NODE) {
            tree->root = y;
        } else if (p->left == z) {
            p->left = y;
        } else {
            p->right = y;
        }
        if (z->left != y) {
            y->left = z->left;
            set_up(y->left, y);
        } else {
            y->left = NULL_NODE;
        }
        if (z->right != y) {
            y->right = z->right;
            set_up(y->right, y);
        } else {
            y->right = NULL_NODE;
        }
        y->color = z->color;
    }
    if (c == RBNODE_COLOR_BLACK)
        remove_balance(tree, x);
    tree->nodes--;
}

static struct rb_node4heap* hrb_tree_search_neareqlarger_loc (
        struct rb_tree4heap *tree, size_t key, int equal)
{
    if (!tree)
        return NULL;

    size_t val1, val2;
    struct rb_node4heap *node = tree->root;
    struct rb_node4heap *found = NULL;

    if (!node || (node == NULL_NODE))
        return found;

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
//            if (node->left == NULL_NODE)
//                return found;
//            val2 = node->left->key;
//            if (val2 < key)
//                return found;
            node = node->left;
        } else {
            node = node->right;
        }
    }
    return found;
}

static struct rb_node4heap* hrb_tree_search_eqlarger_loc (
        struct rb_tree4heap *tree, size_t key, int equal)
{
    if (!tree)
        return NULL;

    size_t val1;
    struct rb_node4heap *node = tree->root;
    struct rb_node4heap *found = NULL;

    if (!node || (node == NULL_NODE))
        return found;

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

    return found;
}

inline struct rb_node4heap* hrb_tree_search_neareqlarger (
        struct rb_tree4heap *tree, size_t key)
{
    return hrb_tree_search_neareqlarger_loc(tree, key, 1);
}

inline struct rb_node4heap* hrb_tree_search_eqlarger (
        struct rb_tree4heap *tree, size_t key)
{
    return hrb_tree_search_eqlarger_loc(tree, key, 1);
}

struct rb_node4heap* hrb_tree_get_neareqlarger (struct rb_node4heap *listroot,
        struct rb_node4heap *node)
{
    struct rb_node4heap *y = node, *u;
    // если есть следующий из двусвязного списка то берем его,
    // иначе берем большего размера из дерева

    if (node->user_bit) {
        y = node->right;
        if (y != NULL) {
            return y;
        }
        // исходный был последним в списке, идем к дереву
        y = listroot;
    } else if (node->dlist != NULL) {
        // корневой элемент списка, берем первый из списка
        return node->dlist;
    }
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
