

#include "palloc.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>



pool_t *create_alloc_pool(size_t small_size,size_t small_n,size_t large_n)
{
    return NULL;
}


int destroy_alloc_pool(pool_t *pool)
{
    return 0;
}

void *palloc(pool_t *pool,size_t size)
{
    return malloc(size);
}


void *pcalloc(pool_t *pool,size_t size)
{
    void *p = palloc(pool,size);
    assert(p);
    memset(p,0,size);
    return p;
}

int pfree(pool_t *pool,void *ptr)
{
    free(ptr);
    return 0;
}