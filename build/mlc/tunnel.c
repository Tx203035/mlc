#include "tunnel.h"
#include "session.h"
#include "connection.h"
#include "event.h"
#include "cycle.h"
#include "core/log.h"
#include "core/chain.h"
#include "core/pool.h"
#include "core/tpool.h"
#include "connection.h"

static int mlc_tunnel_on_session_read(mlc_tunnel_ins_t *ins, chain_t *chain);
static int mlc_tunnel_on_session_break(mlc_tunnel_ins_t *ins);
static int mlc_tunnel_on_tcp_read(connection_t *c, mlc_tunnel_ins_t *ins, chain_t *chain);
static int mlc_tunnel_on_client_session_read(session_t *s, const char *data, uint32_t size, void *userdata);
static int mlc_tunnel_client_on_tcp_accept(connection_t *c, mlc_tunnel_t *tun);
static int mlc_tunnel_on_sever_session_connected( session_t *s ,mlc_tunnel_t *tun);
static int mlc_tunnel_on_server_tcp_read(connection_t *c, mlc_tunnel_ins_t *ins, chain_t *chain);
static int mlc_tunnel_on_session_status(session_t *s,uint8_t state, void *userdata);
static int mlc_tunnel_on_tcp_status(connection_t *c,uint8_t reason);


static int mlc_tunnel_conf_init(mlc_tunnel_conf_t *conf)
{
#define SET_DEFAULT_V(x,v) if(x==0){ x = v;}
#undef SET_DEFAULT_V
    return 0;
}

int mlc_tunnel_client_on_tcp_accept(connection_t *c, mlc_tunnel_t *tun)
{
    log_info(NULL,"c=%d fd=%d",c->index,c->fd);
    c->on_read = (connection_on_read_handler)mlc_tunnel_on_tcp_read;
    c->on_status = (connection_on_status_handler)mlc_tunnel_on_tcp_status;
    cycle_t *cycle = tun->cycle;
    mlc_tunnel_ins_t *ins = mlc_palloc(cycle->pool_small,sizeof(mlc_tunnel_ins_t));
    memset(ins,0,sizeof(mlc_tunnel_ins_t));
    c->userdata = ins;
    connection_set_raw(c);
    ins->tun = tun;
    const mlc_tunnel_conf_t *cf = &tun->conf;
    session_t *s = session_create_client(cycle, &cf->addr_remote);
    s->on_receive = (session_on_receive_handler)mlc_tunnel_on_client_session_read;
    s->on_status = (session_on_status_change_handler)mlc_tunnel_on_session_status;
    s->userdata = ins;
    ins->s = s;
    ins->c = c;
    session_connect(s);

    return 0;
}


int mlc_tunnel_on_tcp_read(connection_t *c, mlc_tunnel_ins_t *ins, chain_t *chain)
{
    // log_info(NULL,"c=%d fd=%d tcp recv chain=%p len=%d ",c->index,c->fd,chain,chain->len);
    if(ins)
    {
        assert(c == ins->c);
        if (c != ins->c) {
            log_info(NULL,"not the same c=%d ins_c=%d",c->index,ins->c->index);
            return 0;
        }
        session_t *s = ins->s;
        int ret = session_data_send(s, chain->data_start, chain->len);
        if(ret < 0)
        {
            log_info(NULL,"c=%d fd=%d tunnel tcp data not sent in session",c->index,c->fd);
            mlc_io_mult_mod(c->cycle,c->event,MLC_EVENT_READ);
            return ret;
        }
    }
    return 0;
}

int mlc_tunnel_on_tcp_status(connection_t *c, uint8_t state)
{
    if (state == mlc_state_disconnect) 
    {
        log_error(NULL,"tcp connect closed! c=%d ",c->index);
        mlc_tunnel_ins_t *ins = c->userdata;
        if(ins)
        {
            cycle_t *cycle = c->cycle;
            session_t *s = ins->s;
            if (s && s->userdata)
            {
                assert(s->userdata == ins);
                s->userdata = NULL;
                session_graceful_close(s, 0);
            }
            mlc_pfree(cycle->pool_small, ins);
            if(c && c->userdata && c->active)
            {
                c->userdata = NULL;
                if(c->state != mlc_state_closed)
                {
                    connection_close(c,MLC_TUNNEL_TCP_DISCONNECT);
                }
            }
        }

    }

    return 0;
}

int mlc_tunnel_on_client_session_read(session_t *s, const char *data, uint32_t size, void *userdata)
{
    // log_info(NULL,"s=%d recv data len=%d",s->index,size);
    mlc_tunnel_ins_t *ins = s->userdata;
    connection_t *c = ins->c;
    cycle_t *cycle = c->cycle;
    int left_size = size;
    const char *p = data;
    while(left_size > 0){
        chain_t *c_raw = chain_alloc(cycle->pool,0,MLC_PKG_MAX_SIZE,mlc_chain_mask_tunnel_read);
        uint32_t send_size = left_size > MLC_PKG_MAX_SIZE ? MLC_PKG_MAX_SIZE : left_size;
        memcpy(c_raw->data_start,p,send_size);
        c_raw->len = send_size;
        connection_data_send(ins->c,c_raw,0);
        chain_release(c_raw,mlc_chain_mask_tunnel_read);
        left_size -= send_size;
        p = p + send_size;
    }
    return 0;
}

int mlc_tunnel_reconnect_tcp(mlc_tunnel_ins_t *ins)
{
    mlc_tunnel_t *tun = ins->tun;
    cycle_t *cycle = tun->cycle;
    const mlc_tunnel_conf_t *cf = &tun->conf;
    connection_t *c = connection_create(cycle, ins, cf->addr_remote.ip, cf->addr_remote.port, MLC_CONNECT_MODE_TCP);
    if (c == NULL)
    {
        log_info(NULL,"tcp connect failed! ins=%p connection create failed", ins);
        return -1; 
    }
    connection_set_read_function(c, (connection_on_read_handler)mlc_tunnel_on_tcp_read);
    connection_set_status_function(c, (connection_on_status_handler)mlc_tunnel_on_tcp_status);
    connection_set_raw(c);
    ins->c = c;

    int ret = connection_reconnect(c);
    if (ret < 0) 
    {
        log_info(NULL,"tcp connect failed! ins=%p c=%d s=%d", ins, ins->s->index);
        return -1;
    }

    log_info(NULL,"tcp connecting c=%d,s=%d,s_op=%d",c->index,ins->s->index,ins->s->index_op);
    return 0;
}

int mlc_tunnel_on_sever_session_connected( session_t *s ,mlc_tunnel_t *tun)
{
    log_info(NULL,"session connected s=%d s_op=%d",s->index,s->index_op);
    cycle_t *cycle = tun->cycle;
    mlc_tunnel_ins_t *ins = mlc_palloc(cycle->pool_small, sizeof(mlc_tunnel_ins_t));
    memset(ins,0,sizeof(mlc_tunnel_ins_t));
    ins->tun = tun;
    ins->s = s;
    ins->c = NULL;
    s->userdata = ins;
    mlc_tunnel_reconnect_tcp(ins);
    return 0;
}

int mlc_tunnel_on_server_session_recv(session_t *s,const char *data, uint32_t size)
{
    mlc_tunnel_ins_t *ins = s->userdata;
    cycle_t *cycle = s->cycle;
    const char *p = data;
    uint32_t left_size = size;
    while(left_size > 0)
    {
        chain_t *chain_raw = chain_alloc(cycle->pool,0,MLC_PKG_MAX_SIZE,mlc_chain_mask_tunnel_write);
        uint32_t cur_size = left_size > MLC_PKG_MAX_SIZE ? MLC_PKG_MAX_SIZE : left_size;
        chain_raw->len = cur_size;
        memcpy(chain_raw->data_start,p,cur_size);
        ins->chain_write = chain_append_one(ins->chain_write, chain_raw);
        p += cur_size;
        left_size -= cur_size;
    }
    
    connection_t *c = ins->c;
    if (c == NULL ) {
        mlc_tunnel_reconnect_tcp(ins);
        c = ins->c;
    }
    if (c) {
        chain_t *chain = ins->chain_write;
        while(chain)
        {
            chain_t *next = chain->next;
            chain->next = NULL;
            int ret = connection_data_send(c,chain,0);
            if (ret >= 0) {
                chain_release(chain,mlc_chain_mask_tunnel_write);
                chain = next;
                ins->chain_write = next;
            }
            else
            {
                chain->next = next;
                ins->chain_write = chain;
                log_info(NULL,"connect send not finish!");
                break;
            }
        }
    }
    return 0;
}


int mlc_tunnel_on_session_status(session_t *s,uint8_t state, void *userdata)
{
    if (state == mlc_state_disconnect || state == mlc_state_closed) 
    {
        log_info(NULL,"session s=%p s=%d s_op=%d closed",s,s->index,s->index_op);
        mlc_tunnel_ins_t *ins = s->userdata;
        if (ins) 
        {
            cycle_t *cycle = s->cycle;
            connection_t *c = ins->c;
            if(c && c->userdata){
                assert(c->userdata == ins);
                c->userdata = NULL;
                connection_close(c,MLC_TUNNEL_SESSION_DISCONNECT);
            }
            s->userdata = NULL;
            session_graceful_close(s,0);
            mlc_pfree(cycle->pool_small, ins);
        }
    }
    return 0;
}


int mlc_tunnel_init_client(mlc_tunnel_t *tun)
{
    cycle_t *cycle = tun->cycle;
    const mlc_tunnel_conf_t *cf = &tun->conf;
    tun->tl = create_tcp_listener(cycle, &cf->addr_listen, tun);
    if (tun->tl == NULL) 
    {
        return -2;
    }
    connection_listener_set_handler(tun->tl, (connection_on_accept_handler)mlc_tunnel_client_on_tcp_accept);
    return 0;
}


static int mlc_tunnel_init_server(mlc_tunnel_t *tun)
{
    cycle_t *cycle = tun->cycle;
    const mlc_tunnel_conf_t *cf = &tun->conf;
    tun->sl = session_listener_create(cycle, &cf->addr_listen);
    session_listener_set_handler(tun->sl, tun,
                                 (session_on_connected_handler)mlc_tunnel_on_sever_session_connected, 
                                 (session_on_receive_handler)mlc_tunnel_on_server_session_recv, 
                                 (session_on_status_change_handler)mlc_tunnel_on_session_status);

    return 0;
}


mlc_tunnel_t *mlc_tunnel_create(cycle_t *cycle, const mlc_tunnel_conf_t *conf)
{
    mlc_tunnel_t *tun = malloc(sizeof(mlc_tunnel_t));
    assert(tun);
    tun->cycle = cycle;
    tun->conf = *conf;
    mlc_tunnel_conf_init(&tun->conf);
    const mlc_tunnel_conf_t *cf = &tun->conf;
    if (cf->is_server==0) 
    {
        /* code */
        mlc_tunnel_init_client(tun);
    }
    else
    {
        mlc_tunnel_init_server(tun);
    }
    return tun;
}

