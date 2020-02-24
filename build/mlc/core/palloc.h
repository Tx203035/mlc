
#ifndef _LIBC91_PALLOC_H_
#define _LIBC91_PALLOC_H_


#include <sys/types.h>
#include "core.h"


struct{
    void *pool_start;
} pool_s;

pool_t *create_alloc_pool(size_t small_size,size_t small_n,size_t large_n);
int destroy_alloc_pool(pool_t *pool);

void *palloc(pool_t *pool,size_t size);
void *pcalloc(pool_t *pool,size_t size);

int pfree(pool_t *pool,void *ptr);

#endif