#ifndef _MLC_CONF_H_
#define _MLC_CONF_H_

#include "core/log.h"
#include "core/core.h"

struct mlc_addr_conf_s
{
	char ip[64];
    uint16_t port;
};

struct rs_conf_s
{
    uint32_t data_shard;
	uint32_t parity_shard;
};

struct cycle_conf_s
{
	uint32_t connection_n;
	uint32_t mtu;
	uint32_t debugger;
	uint32_t pool_block_size;
	uint32_t pool_block_cnt;
	uint32_t pool_small_block_size;
	uint32_t pool_small_block_cnt;
    struct rs_conf_s rs;
    uint32_t heartbeat_check_ms;
    uint32_t heartbeat_timeout_ms;
    uint32_t connect_timeout_ms;
    uint32_t lost_timeout_ms;
	uint32_t session_write_buffer_size;
	uint32_t session_write_queue_size;
	int32_t backlog;
};


struct mlc_tunnel_conf_s
{
    int is_server;
    struct mlc_addr_conf_s addr_listen;
    struct mlc_addr_conf_s addr_remote;
};

//mlc url to session conf
//eg:
// mlc://10.10.5.89:9006?aaa=bbb&xxx=123%dfadsf=$443
MLC_API int mlc_url_to_conf(const char *url,struct mlc_addr_conf_s *conf);

#endif