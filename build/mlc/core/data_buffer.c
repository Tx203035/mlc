#include "data_buffer.h"

static inline uint8_t *align_ptr(uint8_t *p) {
    return (uint8_t *)(((size_t)p + 15) & ~0xF);
}

static void destroy_large(data_buffer_t *buf) {
    for (data_large_t *p = buf->large; p; p = p->next) {
        if (p->alloc) {
            free(p->alloc);
            p->alloc = NULL;
        }
    }
    buf->large = NULL;
}

data_buffer_t *data_buffer_create(uint32_t size, logger_t *log) {
    assert(size > sizeof(data_buffer_t));
    data_buffer_t *p = malloc(size);
    if (!p) {
        return NULL;
    }
    p->log = log;
    p->large = NULL;
    p->current = p;
    p->d.last = (uint8_t *)p + sizeof(data_buffer_t);
    p->d.end = (uint8_t *)p + size;
    p->d.failed = 0;
    p->d.next = NULL;
    size = size - sizeof(data_buffer_t);
    p->max = size < 4096 ? size : 4096;
    return p;
}

int data_buffer_destroy(data_buffer_t *buf) {
    data_buffer_t *p, *n;
    for (p = buf, n = buf->d.next;; p = n, n = n->d.next) {
        destroy_large(p);
        free(p);
        if (n == NULL) {
            break;
        }
    }
    return 0;
}

int data_buffer_reset(data_buffer_t *buf) {
    data_buffer_t *p, *n;
    for (p = buf, n = buf->d.next;; p = n, n = n->d.next) {
        destroy_large(p);
        p->d.last = (uint8_t *)p + sizeof(data_buffer_t);
        if (n == NULL) {
            break;
        }
    }
    return 0;
}

static void *alloc_large(data_buffer_t *buf, uint32_t size) {
    void *p = malloc(size);
    if(!p){
        return NULL;
    }
    data_large_t *large;
    int n = 0;
    for(large = buf->large; large; large = large->next){
        if(large->alloc == NULL){
            large->alloc = p;
            return p;
        }
        if(n++ > 3){
            break;
        }
    }
    large = data_buffer_alloc(buf,sizeof(data_large_t));
    if(large == NULL){
        free(p);
        return NULL;
    }
    large->alloc = p;
    large->next = buf->large;
    buf->large = large;
    return p;
}

static void *alloc_block(data_buffer_t *buf, uint32_t size) {
    size_t buf_size = (size_t)(buf->d.end = (uint8_t *)buf);
    data_buffer_t *new = data_buffer_create(buf_size, buf->log);
    if (!new) {
        return NULL;
    }
    data_buffer_t *p;
    data_buffer_t *current = buf->current;

    for (p = buf; p->d.next; p = p->d.next) {
        if (p->d.failed++ > 4) {
            current = p->d.next;
            //失败4次以上移动current指针
        }
    }
    p->d.next = new;
    buf->current = current ? current : new;

    data_node_t *d = &new->d;
    uint8_t *m = align_ptr(d->last);
    d->last = m + size;
    return m;
}

void *data_buffer_alloc(data_buffer_t *buf, uint32_t size) {
    if (size < buf->max) {
        data_buffer_t *p = buf->current;
        do {
            data_node_t *d = &p->d;
            uint8_t *m = align_ptr(d->last);
            if (d->end - m >= size) {
                d->last = m + size;
                return m;
            }
            p = d->next;
        } while (p);
        return alloc_block(buf, size);
    }
    return alloc_large(buf, size);
}
