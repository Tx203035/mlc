#include "chain.h"
#include <stdlib.h>
#include <string.h>
#include "pool.h"
#include "util.h"
#include "log.h"

#define TRACE_CHAIN(c,fmt,...) log_info(NULL,"[chain]pool_index=%d p=%p len=%u mask=0x%x|"fmt,\
    c->index_debug,c,c->len,c->mask, ##__VA_ARGS__)

chain_t *chain_alloc_impl(mlc_pool_t *pool, uint32_t data_len, uint32_t capacity, uint32_t mask,int line)
{
    uint32_t c_size = capacity > data_len ? capacity : data_len;
    chain_t *nc = mlc_palloc(pool, c_size + sizeof(chain_t));
    nc->next = NULL;
    nc->capacity = c_size;
    nc->pool = pool;
    nc->len = data_len;
    nc->mask = mask;
    nc->debug = line;
    memset(nc->data_start, 0, capacity);
    TRACE_CHAIN(nc,"alloc");
    return nc;
}

chain_t *chain_clone_one(const chain_t *chain, uint32_t mask)
{
    // assert(chain->next == NULL);
    chain_t *new_chain = chain_alloc(chain->pool, chain->len, chain->capacity, mask);
    if (new_chain == NULL)
    {
        return NULL;
    }
    memcpy(new_chain->data_start, chain->data_start, chain->len);
    return new_chain;
}

chain_t *chain_append_one(chain_t *chain,chain_t *chain_new)
{
    assert(chain_new->next == 0);
    chain_new->next = 0;
    return chain_append(chain,chain_new);
}

chain_t *chain_append(chain_t *chain, chain_t *chain_new)
{
    chain_t *last_c = NULL;
    for (chain_t *c = chain; c != NULL; c = c->next)
    {
        assert(c != chain_new);
        if (c == chain_new)
        {
            return chain;
        }
        last_c = c;
    }

    if (last_c)
    {
        last_c->next = chain_new;
    }
    
    return chain != NULL ? chain : chain_new;
}

chain_t *chain_release_one(chain_t* chain, uint32_t mask)
{
    chain_t *next = chain->next;
    TRACE_CHAIN(chain,"remove mask=0x%x %d", mask,mlc_shift_1_n(mask) );
    chain->mask &= ~mask;
    if (chain->mask == 0)
    {
        TRACE_CHAIN(chain,"release chain,mask=0x%x %d", mask, mlc_shift_1_n(mask));
        assert(chain->index_debug < chain->pool->block_cnt && "chain cannot release twice");
        mlc_pfree(chain->pool,chain);
    }
    return next;
}

chain_t *chain_split(chain_t *chain, uint32_t len)
{
    if (chain->len <= len)
    {
        return NULL;
    }
    int left_len = chain->len - len;
    chain_t *new_chain = chain_alloc(chain->pool, left_len, chain->capacity, chain->mask);
    assert(new_chain);
    chain->len = len;
    memcpy(new_chain->data_start, chain->data_start + len, left_len);
    return new_chain;
}

chain_t *chain_release(chain_t *chain,uint32_t mask)
{
    chain_t *c = chain;
    chain_t *next = NULL;
    while (c)
    {
        c = chain_release_one(c, mask);
    }
    return c;
}