#include "dqueue.h"

dequeue_ptr_t *dqueue_ptr_create(uint32_t max) {
    dequeue_ptr_t *q = malloc(sizeof(dequeue_ptr_t) + max * sizeof(void *));
    q->max = max;
    q->head = 0;
    q->tail = 0;
    q->size = 0;
    return q;
}

int dqueue_ptr_destroy(dequeue_ptr_t *q)
{
    free(q);
    return 0;
}