#include "tpool.h"
#include "util.h"
#include "log.h"

int tpool_trace(logger_t *l,tpool_t *pool)
{
    log_trace(l,"[tpool]total=%d,free=%d,used=%d,objsize=%d",pool->capacity, pool->free, pool->used,pool->obj_size);
    return 0;
}


tpool_t *tpool_create(size_t capacity, size_t obj_size, void *init_conf, tpool_init_func fun_init, tpool_destroy_func fun_destroy, tpool_reset_func fun_reset)
{
    tpool_t *pool = malloc(sizeof(tpool_t));
    assert(pool);
    if (pool == NULL) 
    {
        return NULL;
    }

    size_t objsize = mlc_align(obj_size + sizeof(tpool_obj_t));
    pool->obj_pool = malloc(capacity * objsize);
    assert(pool->obj_pool);
    if (pool->obj_pool == NULL)
    {
        free(pool);
        return NULL;
    }

    pool->obj_pool_end = (tpool_obj_t *)((char *)pool->obj_pool + capacity * objsize);
    pool->obj_size = objsize;
    pool->free_head = NULL;
    for (size_t i = 0; i < capacity; i++)
    {
        tpool_obj_t *p = (tpool_obj_t *)((char *)pool->obj_pool + objsize * i);
        fun_init(p->data, init_conf, i + 1);
        p->index = i;
        p->xx = 0x55aa55aa;
        p->next = pool->free_head;
        pool->free_head = p;
    }
    pool->release_head = NULL;
    pool->fun_init = fun_init;
    pool->fun_destroy = fun_destroy;
    pool->fun_reset = fun_reset;
    pool->free = capacity;
    pool->used = 0;
    pool->capacity = capacity;
    return pool;
}

void *tpool_get_free(tpool_t *pool)
{
    tpool_obj_t *obj = pool->free_head;
    assert(obj && "alloc failed");
    if(obj == NULL)
    {
        return NULL;
    }
    pool->free_head = obj->next;
    assert(pool->free_head);
    obj->next = NULL;
    pool->free -= 1;
    pool->used += 1;
    return (void *)obj->data;
}

void tpool_update(tpool_t *pool)
{
    while (pool->release_head)
    {
        tpool_obj_t *obj = pool->release_head;
        pool->release_head = obj->next;
        obj->next = pool->free_head;
        pool->free_head = obj;
        pool->fun_reset(obj->data);
        pool->free += 1;
        pool->used -= 1;
    }
}

void tpool_release(tpool_t *pool, void *ptr)
{
    tpool_obj_t *obj = (tpool_obj_t *)((char *)ptr - mlc_offset_of(tpool_obj_t, data));
    assert(obj && "release null");
    assert(obj->next == 0 && "release twice");
    assert(obj >= pool->obj_pool && obj < pool->obj_pool_end && "not in this poll");
    assert(obj->index>0 && obj->index<pool->capacity && obj->xx==0x55aa55aa && "memery override");
    
    obj->next = pool->release_head;
    pool->release_head = obj;
    
    return;
}



void *tpool_get_by_index(tpool_t *pool, int index)
{
    if (index <= 0 || index > pool->capacity)
    {
        return NULL;
    }
    tpool_obj_t *p = (tpool_obj_t *)((char*)pool->obj_pool + pool->obj_size * (index - 1));
    return (void*)p->data;
}

int tpool_destroy(tpool_t *pool)
{
    assert(pool);
    for (size_t i = 0; i < pool->capacity; i++)
    {
        tpool_obj_t *p = (tpool_obj_t *)((char *)pool->obj_pool + pool->obj_size * i);
        pool->fun_destroy(p->data);
        p->next = pool->free_head;
        pool->free_head = p;
    }
    free(pool->obj_pool);
    free(pool);
    return 0;
}