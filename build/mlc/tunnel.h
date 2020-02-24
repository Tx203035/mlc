#ifndef _MLC_TUNNEL_H_
#define _MLC_TUNNEL_H_

#include "mlc.h"

struct mlc_tunnel_s
{
    cycle_t *cycle;
    mlc_tunnel_conf_t conf;
    connection_listener_t *ul;
    session_listener_t *sl;
    connection_listener_t *tl;
};

struct mlc_tunnel_ins_s
{
    mlc_tunnel_t *tun;
    session_t *s;
    connection_t *c;
    chain_t *chain_read;
    chain_t *chain_write;
};


MLC_API mlc_tunnel_t *mlc_tunnel_create(cycle_t *cycle, const mlc_tunnel_conf_t *conf);


#endif