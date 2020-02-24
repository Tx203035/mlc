#ifndef _LIBMLC_BUFFER_H_
#define _LIBMLC_BUFFER_H_

#include "core.h"

enum
{
    mlc_chain_mask_connection_write = 1,
    mlc_chain_mask_session_write = 1 << 1,
    mlc_chain_mask_kcp_write = 1 << 2,
    mlc_chain_mask_fec_write = 1 << 3,
    mlc_chain_mask_connection_read = 1 << 4,
    mlc_chain_mask_session_read = 1 << 5,
    mlc_chain_mask_kcp_read = 1 << 6,
    mlc_chain_mask_fec_read = 1 << 7,
    mlc_chain_mask_fec_group = 1 << 8,
    mlc_chain_mask_tunnel_read = 1 << 9,
    mlc_chain_mask_tunnel_write = 1 << 10,
    mlc_chain_mask_debug = 1 << 31,
};

struct chain_s
{
    union
    {
        mlc_pool_data_node_t *next_in_pool;
        int index_debug;
    };    //mustbe same with mlc_pool_data_node
    uint32_t capacity;
    mlc_pool_t *pool;
    chain_t *next;
    uint32_t mask;
    uint32_t len;
    uint32_t debug;
    char data_start[0];
};

static inline chain_t *chain_retain_one(chain_t *c, uint32_t mask)
{
    c->mask |= mask;
    return c;
}

#define chain_alloc(pool, data_len, capacity, mask) chain_alloc_impl(pool,data_len,capacity,mask,__LINE__);
MLC_API chain_t *chain_alloc_impl(mlc_pool_t *pool, uint32_t data_len, uint32_t capacity, uint32_t mask,int line);
MLC_API chain_t *chain_clone_one(const chain_t *chain, uint32_t mask);
MLC_API chain_t *chain_append(chain_t *chain,chain_t *chain_new);
MLC_API chain_t *chain_append_one(chain_t *chain,chain_t *chain_new);
MLC_API chain_t *chain_release_one(chain_t* chain, uint32_t mask);
MLC_API chain_t *chain_split(chain_t *chain, uint32_t len);

MLC_API chain_t *chain_release(chain_t *chain, uint32_t mask);
MLC_API chain_t *chain_release_one(chain_t *chain, uint32_t mask);

#endif