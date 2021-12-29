/** \brief double-linked list (two-way list) */

#ifndef DLIST_H
#define DLIST_H

#include <os_types.h>

struct dlist {
    struct dlist *next;
    struct dlist *prev;
    void *entry;
};

static inline bool dlist_empty (struct dlist *node)
{
    return node->next == NULL && node->prev == NULL ? true : false;
}

static inline void dlist_init (struct dlist *node)
{
    node->next = NULL;
    node->prev = NULL;
    node->entry = NULL;
}

static inline void* dlist_get_entry (struct dlist *node)
{
    return node->entry;
}

static inline void dlist_set_entry (struct dlist *node, void *entry)
{
    node->entry = entry;
}

static inline void dlist_insert (struct dlist *prev, struct dlist *node)
{
    node->prev = prev;
    node->next = prev->next;
    if (prev->next != NULL)
        prev->next->prev = node;
    prev->next = node;
}

static inline void dlist_remove (struct dlist *node)
{
    if (node->prev != NULL)
        node->prev->next = node->next;
    if (node->next != NULL)
        node->next->prev = node->prev;
}

static inline struct dlist* dlist_find (struct dlist *node, void *entry)
{
    if (node == NULL)
        return NULL;

    while (node->entry != entry) {
        if (!node->next)
            return NULL;
        node = node->next;
    }
    return node;
}

#endif
