#include "pool.h"
#include "chain.h"
#include "log.h"
#include "util.h"


static inline int debug_ptr_to_index(mlc_pool_t *pool, void *p)
{
    return ((char *)p - (char *)pool->start)/ pool->block_size;
}

void dump_as_chain(mlc_pool_t *pool)
{
    log_warn(NULL,"[pool]total=%d,left=%d,alloc=%d,max_usage=%d",pool->block_cnt,pool->free_n,pool->alloc_n,pool->alloc_max);
    log_warn(NULL,"[pool]dump memory as chain: start=%p end=%p",pool->start,pool->end);
    int cnt = 0;
    for (size_t i = 0; i < pool->block_cnt; i++)
    {
        mlc_pool_data_node_t *p = (mlc_pool_data_node_t *)((char *)pool->start + i * pool->block_size);
        if(p->next < pool->start || p->next >= pool->end)
        {
            chain_t *chain = (chain_t *)p;
            log_warn(NULL,"[pool]%p i=%d id=%d:mask=%x %d debug=%d|n=%p ni=%d ",p,debug_ptr_to_index(pool,p),chain->index_debug,
                chain->mask, mlc_shift_1_n(chain->mask),chain->debug, p->next,debug_ptr_to_index(pool,p->next));   
            ++cnt;         
        }
    }
    log_warn(NULL,"[pool]dump done, cnt=%d\n", cnt);
}

void mlc_pool_trace(logger_t *l, mlc_pool_t *pool)
{
    log_trace(l,"[pool]blocksize=%d,total=%d,used=%d,left=%d,alloc=%d,max_usage=%d",
        pool->block_size,pool->block_cnt, pool->block_cnt - pool->free_n ,pool->free_n,pool->alloc_n,pool->alloc_max);
}

mlc_pool_t *mlc_create_pool(size_t block_size,size_t block_cnt)
{
    mlc_pool_data_node_t *p_last = NULL;
    size_t len = block_size * block_cnt;
    size_t total_len = sizeof(mlc_pool_t) + len;

    assert(block_size % sizeof(void*) == 0);
    assert(block_cnt % sizeof(void*) == 0);

    mlc_pool_t *pool = malloc(total_len);//这里需要alloc align 对齐
    assert(((char *)pool->start - (char *)0) % sizeof(mlc_pool_data_node_t) == 0);
    memset(pool, 0, total_len);
    pool->block_size = block_size;
    pool->block_cnt = block_cnt;
    pool->end = (mlc_pool_data_node_t *)((char *)pool->start + len);
    for (size_t i = 0; i < block_cnt; i++)
    {
        mlc_pool_data_node_t *p = (mlc_pool_data_node_t *)((char *)pool->start + i * block_size);
        p->next = p_last;
        p_last = p;
    }
    pool->head_free = p_last;
    pool->free_n = block_cnt;
    pool->alloc_n = 0;
    pool->alloc_max = 0;
    assert(mlc_pfree(pool,mlc_palloc(pool, 1))==0);

    // mlc_pool_data_node_t *p = pool->head_free;
    // size_t cnt = 0;
    // while(p)
    // {
    //     ++cnt;
    //     p = p->next;
    // }
    // assert(cnt == block_cnt);

    return pool;
}


void *mlc_palloc(mlc_pool_t *pool,size_t size)
{
    assert(size <= pool->block_size);
    if (size > pool->block_size)
    {
        return NULL;
    }
    
    mlc_pool_data_node_t *node = pool->head_free;
    if (node == NULL) 
    {
        dump_as_chain(pool);
    }
    else
    {
        pool->free_n -= 1;
        pool->alloc_n += 1;
        pool->head_free = node->next;
        node->index_debug = debug_ptr_to_index(pool,node);
        int used = pool->block_cnt - pool->free_n;
        if (used > pool->alloc_max) 
        {
            pool->alloc_max = used;
        }        
    }
    assert(node && "alloc failed");
    return (void *)node;
}


int mlc_pfree(mlc_pool_t *pool,void *ptr)
{
    mlc_pool_data_node_t *node = (mlc_pool_data_node_t *)ptr;
    if((char *)ptr < (char *)pool->start || (char *)ptr >= (char *)pool->end) //必须是自己池子的
    {
        assert(0 && "must be in pool");
        return -1;
    }
    if (((char *)ptr - (char *)pool->start) % pool->block_size != 0) //必须是node的指针不能偏移错误
    {
        assert(0 && "may be wild ptr"); 
        return -2;
    }

    node->next = pool->head_free;
    pool->head_free = node;
    pool->free_n += 1;
    return 0;
}