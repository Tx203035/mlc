#ifndef _MLC_POOL_H_
#define _MLC_POOL_H_

#include "core.h"

struct mlc_pool_data_node_s
{
    union
    {
        mlc_pool_data_node_t *next;
        int index_debug;
    };
};


struct mlc_pool_s
{                                /* 内存池的管理模块，即内存池头部结构 */
    mlc_pool_data_node_t *head_free;  /* 当前内存分配的结束位置，即下一段可分配内存的起始位置 */
    unsigned int failed; /* 记录内存池内存分配失败的次数 */
    size_t block_size;                  /* 内存池数据块的最大值 */
    size_t block_cnt;
    int alloc_n;
    int alloc_max;
    int free_n;
    // mlc_pool_t *current;         /* 指向当前内存池 */
    // mlc_pool_large_t *large;     /* 大块内存链表，即分配空间超过 max 的内存 */
    // mlc_pool_cleanup_t *cleanup; /* 析构函数，释放内存池 */
    // ngx_log_t *log;              /* 内存分配相关的日志信息 */
    mlc_pool_data_node_t *end;
    mlc_pool_data_node_t start[0];
};

// struct mlc_pool_large_s
// {
//     mlc_pool_large_t *next;
//     void *alloc;
// };

mlc_pool_t *mlc_create_pool(size_t block_size,size_t block_cnt);
void *mlc_palloc(mlc_pool_t *pool,size_t size);
int mlc_pfree(mlc_pool_t *pool,void *ptr);
void mlc_pool_trace(logger_t * l,mlc_pool_t *pool);

#endif