#ifndef _MLC_QUEUE_H_INCLUDED_
#define _MLC_QUEUE_H_INCLUDED_

#include "core.h"

typedef struct mlc_queue_s mlc_queue_t;

struct mlc_queue_s
{
    mlc_queue_t *prev;
    mlc_queue_t *next;
};

#define mlc_queue_init(q) \
    (q)->prev = q;        \
    (q)->next = q

#define mlc_queue_empty(h) \
    (h == (h)->prev)

#define mlc_queue_insert_head(h, x) \
    (x)->next = (h)->next;          \
    (x)->next->prev = x;            \
    (x)->prev = h;                  \
    (h)->next = x

#define mlc_queue_insert_after mlc_queue_insert_head

#define mlc_queue_insert_tail(h, x) \
    (x)->prev = (h)->prev;          \
    (x)->prev->next = x;            \
    (x)->next = h;                  \
    (h)->prev = x

#define mlc_queue_head(h) \
    (h)->next

#define mlc_queue_last(h) \
    (h)->prev

#define mlc_queue_sentinel(h) \
    (h)

#define mlc_queue_next(q) \
    (q)->next

#define mlc_queue_prev(q) \
    (q)->prev

#define mlc_queue_remove(x)      \
    (x)->next->prev = (x)->prev; \
    (x)->prev->next = (x)->next

#define mlc_queue_split(h, q, n) \
    (n)->prev = (h)->prev;       \
    (n)->prev->next = n;         \
    (n)->next = q;               \
    (h)->prev = (q)->prev;       \
    (h)->prev->next = h;         \
    (q)->prev = n;

#define mlc_queue_add(h, n)      \
    (h)->prev->next = (n)->next; \
    (n)->next->prev = (h)->prev; \
    (h)->prev = (n)->prev;       \
    (h)->prev->next = h;

#define mlc_offsetof(type, member) ((size_t) & ((type *)0)->member)

#define mlc_containerof(ptr, type, member)  ((type *)(((char *)((type *)ptr)) - mlc_offsetof(type, member)))

#define mlc_queue_entry(ptr, type, member) mlc_containerof(ptr, type, member)

#define mlc_queue_data(q, type, link) \
    (type *)((u_char *)q - mlc_offsetof(type, link))

#endif /* _MLC_QUEUE_H_INCLUDED_ */
