#ifndef _MLC_DEQUEUE_H_
#define _MLC_DEQUEUE_H_

#include "core.h"

typedef struct dqueue_ptr_s {
    int head;
    int tail;
    int max;
    int size;
    void *data[0];
} dequeue_ptr_t;

dequeue_ptr_t *dqueue_ptr_create(uint32_t max);
int dqueue_ptr_destroy(dequeue_ptr_t *q);

static inline int dequeue_ptr_size(dequeue_ptr_t *q) { return q->size; }

static inline int dqueue_ptr_push_back(dequeue_ptr_t *q, void *ptr) {
    if (q->size < q->max) {
        q->data[q->tail] = ptr;
        int ret = q->tail;
        q->tail = (q->tail + 1) % q->max;
        q->size += 1;
        return ret;
    }
    return -1;
}

static inline int dqueue_ptr_push_front(dequeue_ptr_t *q, void *ptr) {
    if (q->size < q->max) {
        q->head = (q->head - 1 + q->max) % q->max;
        q->data[q->head] = ptr;
        q->size += 1;
        return q->head;
    }
    return -1;
}

static inline void *dqueue_ptr_front(dequeue_ptr_t *q) {
    return q->size > 0 ? q->data[q->head] : NULL;
}

static inline void *dqueue_ptr_back(dequeue_ptr_t *q) {
    int index = (q->tail - 1 + q->max) % q->max;
    return q->size > 0 ? q->data[index] : NULL;
}

static inline void *dequeue_ptr_pop_front(dequeue_ptr_t *q) {
    void *p = NULL;
    if (q->size > 0) {
        p = q->data[q->head];
        q->head = (q->head + 1) % q->max;
        q->size -= 1;
    }
    return p;
}

static inline void *dequeue_ptr_pop_back(dequeue_ptr_t *q) {
    void *p = NULL;
    if (q->size > 0) {
        q->tail = (q->tail - 1 + q->max) % q->max;
        p = q->data[q->tail];
        q->tail = (q->tail - 1 + q->max) % q->max;
        q->size -= 1;
    }
    return p;
}

static inline void dqueue_ptr_reset(dqueue_ptr_t *p){
    p->head = 0;
    p->tail = 0;
    p->size = 0;
}



#endif