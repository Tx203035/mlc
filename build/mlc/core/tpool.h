#ifndef _MLC_TPOOL_H_
#define _MLC_TPOOL_H_


#include "core.h"
#include "queue.h"

typedef int (*tpool_init_func)(void* data, void *init_conf, int index);
typedef int (*tpool_destroy_func)(void *data);
typedef int (*tpool_reset_func)(void *data);

struct tpool_obj_s
{
    uint32_t index;
    uint32_t xx;
    tpool_obj_t *next;
    char data[0];
};

struct tpool_s
{
    int capacity;
    int used;
    int free;
    size_t obj_size;
    tpool_obj_t *obj_pool;
    tpool_obj_t *obj_pool_end;
    tpool_obj_t *free_head;
    tpool_obj_t *release_head;
    tpool_init_func fun_init;
    tpool_destroy_func fun_destroy;
    tpool_reset_func fun_reset;
};

tpool_t *tpool_create(size_t capacity, size_t obj_size, void *init_conf, tpool_init_func fun_init, tpool_destroy_func fun_destroy, tpool_reset_func fun_reset);

void tpool_update(tpool_t *pool);

void *tpool_get_free(tpool_t *pool);
void tpool_release(tpool_t *pool, void *obj);
void *tpool_get_by_index(tpool_t *pool, int index);

int tpool_destroy(tpool_t *pool);
int tpool_trace(logger_t *l,tpool_t *pool);

#endif