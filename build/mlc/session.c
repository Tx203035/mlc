#include "session.h"
#include "connection.h"
#include "cycle.h"
#include "core/chain.h"
#include "core/tpool.h"
#include "kcp.h"
#include "core/util.h"
#include "core/socket_util.h"
#include "core/log.h"
#include "fec.h"
#include "event_timer.h"
#include "core/encoding.h"
#include "core/data_buffer.h"
#include "core/dqueue.h"
#include "sinfo.h"


#define TRACE_SESSION(s,fmt,...) log_info(L_SES(s->cycle),"[session]s=%d s_op=%d state=%d|"fmt,\
    s->index, s->index_op, s->state, ##__VA_ARGS__)

#define WARN_SESSION(s,fmt,...) log_warn(L_SES(s->cycle),"[session]s=%d s_op=%d state=%d|"fmt,\
    s->index, s->index_op, s->state, ##__VA_ARGS__)

#define ERROR_SESSION(s,fmt,...) log_error(L_SES(s->cycle),"[session]s=%d s_op=%d state=%d|"fmt,\
    s->index, s->index_op, s->state, ##__VA_ARGS__)

static int on_fec_input(chain_t *chain, session_t *s);
static int on_fec_output(chain_t *chain, session_t *s);
static int on_kcp_output(chain_t *chain, session_t *s);
static int on_kcp_dead(session_t *s);
static int on_kcp_input(chain_t *chain, session_t *s);

static int session_update(session_t *s);
static int session_state_update(session_t *s);
static void session_active(session_t *s);
static int session_choose_line(session_t *s);
static int session_flush_write(session_t *s);

static connection_t *find_connection(session_t *s, uint8_t state);

static int state_update_on_connecting_s(session_t *s);
static int state_update_on_disconnected_s(session_t *s);
static int state_update_on_connecting_c(session_t *s);
static int state_update_on_connected_s(session_t *s);
static int state_update_on_connected_c(session_t *s);
static int state_update_on_close_wait(session_t *s);

static int session_stop(session_t *s, int reason);


typedef int (*state_func)(void *data);
typedef struct state_obj_s
{
    state_func on_enter;
    state_func on_update;
    state_func on_exit;
}state_obj_t;

static state_obj_t state_table_c[] = 
{
/*init*/            {NULL,NULL,NULL},
/*connecting*/      {NULL, (state_func)state_update_on_connecting_c, NULL},
/*connected*/       {NULL, (state_func)state_update_on_connected_c, NULL},
/*disconnect*/      {NULL,NULL,NULL},
/*lost*/            {NULL,NULL,NULL},
/*close_wait*/      {NULL,NULL,NULL},
/*closed*/          {NULL,NULL,NULL},      
};

static state_obj_t state_table_s[] = 
{
/*init*/            {NULL,NULL,NULL},
/*connecting*/      {NULL, (state_func)state_update_on_connecting_s, NULL},
/*connected*/       {NULL, (state_func)state_update_on_connected_s, NULL},
/*disconnect*/      {NULL,(state_func)state_update_on_disconnected_s,NULL},
/*lost*/            {NULL,NULL,NULL},
/*close_wait*/      {NULL,NULL,NULL},
/*closed*/          {NULL,NULL,NULL},     
};


void session_change_state(session_t *s, uint8_t state,int reason)
{
    if (state > mlc_state_closed)
    {
        TRACE_SESSION(s, "state change error, new state:%d out of range reason=%d", state,reason);
        return;
    }
    // assert(state != s->state);
    if(state == s->state)
    {
        TRACE_SESSION(s,"state change to same %d",reason);
        return;
    }

    cycle_t *cycle = s->cycle;
    TRACE_SESSION(s, "state change from %d to %d, is_server=%d reason=%d", s->state, state, cycle->is_server,reason);

    state_func on_exit = cycle->is_server ? state_table_s[s->state].on_exit : state_table_c[s->state].on_exit;
    state_func on_enter = cycle->is_server ? state_table_s[state].on_enter : state_table_c[state].on_enter;

    if (on_exit)
    {
        on_exit(s);
    }
    s->state = state;
    if (on_enter)
    {
        on_enter(s);
    }

    if (s->on_status) 
    {
        s->on_status(s, state, s->userdata);
    }
}

static int session_init_kcp(session_t *s)
{
    if (s->kcp != NULL)
    {
        kcp_release(s->kcp);
        s->kcp = NULL;
    }

    cycle_t *cycle = s->cycle;
    kcp_pcb_t *kcp = kcp_create(cycle, s);
    assert(kcp);

    kcp_setmtu(kcp, cycle->conf.mtu);
    kcp_nodelay(kcp, 1, 10, 2, 1);
    kcp_wndsize(kcp, 128, 128);
    kcp->output = (kcp_inout_handler)on_kcp_output;
    kcp->input = (kcp_inout_handler)on_kcp_input;
    kcp->dead = (kcp_dead_handler)on_kcp_dead;
    s->kcp = kcp;
    return 0;
}

static int session_init_fec(session_t *s)
{
    if (s->fec != NULL)
    {
        fec_release(s->fec);
        s->fec = NULL; 
    }

    cycle_t *cycle = s->cycle;
    fec_pcb_t *fec = fec_create(cycle, s, 3, 2);
    assert(fec);

    fec->output = (fec_inout_handler)on_fec_output;
    fec->input = (fec_inout_handler)on_fec_input;
    s->fec = fec;
    return 0;
}

int session_init(session_t *s, cycle_t *cycle, int index)
{
    memset(s, 0, sizeof(session_t));
    const cycle_conf_t *conf = &cycle->conf;
    s->cycle = cycle;
    s->index = index;
    s->mss = 1024;
    s->enable_kcp = 1;
    s->enable_fec = 1;
    s->state_update = event_timer_create(cycle->timer_hub, s, (event_timer_handler_pt)session_state_update);
    s->timer_update = event_timer_create(cycle->timer_hub, s , (event_timer_handler_pt)session_update);
    s->timer_update_delay = event_timer_create(cycle->timer_hub, s , (event_timer_handler_pt)session_update);
    s->write_buffer = data_buffer_create(conf->session_write_buffer_size,L_SES(cycle));
    s->write_queue = dqueue_ptr_create(conf->session_write_queue_size);

    session_init_kcp(s);
    session_init_fec(s);

    return 0;
}

int session_reset(session_t *s)
{
    cycle_t *cycle = s->cycle;
    if (s->read_chain) 
    {
        s->read_chain = chain_release(s->read_chain, mlc_chain_mask_session_read);
    }
    if (s->write_chain) 
    {
        s->write_chain = chain_release(s->write_chain, mlc_chain_mask_session_write);
    }
    if (s->temp_chain)
    {
        s->temp_chain = chain_release(s->temp_chain, mlc_chain_mask_session_write);
    }
    if(s->match)
    {
        sinfo_delete(cycle->sc,s->match);
    }
    data_buffer_reset(s->write_buffer);
    dqueue_ptr_reset(s->write_queue);

    s->match = 0;
    s->read_chain = NULL;
    s->write_chain = NULL;
    s->temp_chain = NULL;
    s->on_receive = NULL;
    s->on_status = NULL;
    s->userdata = NULL;
    s->index_op = 0;
    s->snd_nxt = 0;
    s->rcv_nxt = 0;
    s->read_chain_n = 0;
    s->write_chain_n = 0;
    s->temp_chain_n = 0;
    s->start_time_ms = 0;
    s->next_kcp_update_ms = 0;
    s->active = 0;
    s->send_busy = 0;
    s->state = mlc_state_init;

    session_init_kcp(s);
    session_init_fec(s);

    return 0;
}

int session_release(session_t *s)
{
    session_reset(s);
    event_timer_release(s->state_update);
    event_timer_release(s->timer_update);
    event_timer_release(s->timer_update_delay);
    kcp_release(s->kcp);
    fec_release(s->fec);
    return 0;
}

int session_graceful_close(session_t *s, int reason)
{
    if (s->state != mlc_state_closed && s->state !=mlc_state_close_wait)
    {
        TRACE_SESSION(s,"graceful close reason=%d",reason);
        for (int i = 0; i < MAX_SESSION_CONNECTIONS; i++)
        {
            connection_t *c = s->connections[i];
            if (c && c->state == mlc_state_connected)
            {
                session_send_ctl(s, c, mlc_control_close_req);
            }
        } 

        if (s->state != mlc_state_close_wait)
        {
            session_change_state(s, mlc_state_close_wait, __LINE__);
        }        
    }
    else
    {
        TRACE_SESSION(s, "graceful close inprogress reason=%d", reason);
        session_close(s, reason);
    }

    return 0;
}

int session_close(session_t *s, int reason)
{
    TRACE_SESSION(s, "session close, reason=%d", reason);
    if (s->active && s->state!=mlc_state_closed)
    {
        s->active = 0;
        session_stop(s, reason);
        cycle_t *cycle = s->cycle;
        tpool_release(cycle->sm, s);
    }
    return 0;
}

int session_stop(session_t *s, int reason)
{
    TRACE_SESSION(s, "session stop, reason=%d", reason);
    for (int i = 0; i < MAX_SESSION_CONNECTIONS; i++)
    {
        connection_t *c = s->connections[i];
        if (c == NULL)
        {
            continue;
        }
        connection_close(c, reason);
        TRACE_SESSION(s, "close connection[%d], reason[%d]", c->index, reason);
        s->connections[i] = NULL;
    }

    if (s->state_update->started)
    {
        event_timer_stop(s->state_update);
    }
    if (s->timer_update->started) 
    {
        event_timer_stop(s->timer_update);
    }
    if(s->timer_update_delay->started)
    {
        event_timer_stop(s->timer_update_delay);
    }

    session_change_state(s, mlc_state_closed, __LINE__);
    return 0;
}

static int on_fec_input(chain_t *chain,session_t *s)
{
    session_active(s);
    int ret = kcp_input(s->kcp, chain);
    if (ret < 0)
    {
        /* code */
        TRACE_SESSION(s,"kcp_input error=%d", ret);
    }
    return 0;
}

int on_fec_output(chain_t *chain, session_t *s)
{
    int line = session_choose_line(s);
    if (line >= 0)
    {
        connection_t *c = s->connections[line];
        assert(c);
        return connection_data_send(c, chain, MLC_PKG_TYPE_DATA);
    }
    else
    {
        if(s->state == mlc_state_connected){
            // TRACE_SESSION(s, "no connected connection", s->index, s->index_op);
        }
        return -1;
    }
}

static int on_kcp_output(chain_t *chain, session_t *s)
{
    int enable_fec = s->enable_fec && s->state == mlc_state_connected;
    return fec_send(s->fec, chain, enable_fec);
}

static int on_kcp_dead(session_t *s)
{
    if (s->state == mlc_state_connected) {
        TRACE_SESSION(s, "kcpdead when connected");
        session_change_state(s,mlc_state_disconnect,__LINE__);
    }
    return 0;
}

static int on_kcp_input(chain_t *chain, session_t *s)
{
    if (s->state != mlc_state_connected)
    {
        TRACE_SESSION(s, "state fail to recv");
        return 0;
    }

    chain_retain_one(chain, mlc_chain_mask_session_read);
    mlc_pkg_session_t *sdata = &chain_to_data(chain)->session;
    s->rcv_nxt = sdata->sn;
    s->read_chain = chain_append_one(s->read_chain, chain);
    s->read_chain_n += 1;
    //chain_release(chain, mlc_chain_mask_session_read);
    // pkg_debug(chain, "session recv");
    assert(chain->next == NULL);
    if (s->on_receive)
    {
        int size = session_recv(s, s->recv_buffer, MLC_DATA_MAX_SIZE);
        if (size > 0) 
        {
            s->on_receive(s, s->recv_buffer, size, s->userdata);
        }
    }
    return 0;
}

int session_input(session_t *s, chain_t *chain)
{
    return 0;
}

int session_send_ctl(session_t *s, connection_t *c, uint8_t ctl_code)
{
    cycle_t *cycle = c->cycle;
    chain_t *chain = chain_alloc(cycle->pool, sizeof(mlc_pkg_ctl_t), 0,
                                 mlc_chain_mask_session_write);
    if (chain == NULL)
    {
        return -1;
    }

    mlc_pkg_ctl_t *ctl = chain_to_ctl(chain);
    memset(ctl, 0, sizeof(mlc_pkg_ctl_t));
    ctl->body.control = ctl_code;
    if (s) 
    {
        ctl->session.health = s->health;
        ctl->body.index_op = s->index_op;
        ctl->body.index = s->index;
    }
    int ret = connection_data_send(c, chain, MLC_PKG_TYPE_CTL);
    chain_release(chain,mlc_chain_mask_session_write);
    return ret;
}

static int session_server_send_heartbeat(session_t *s, connection_t *c,const mlc_pkg_body_ctl_t *ctl_client)
{
    cycle_t *cycle = s->cycle;
    chain_t *chain = chain_alloc(cycle->pool, sizeof(mlc_pkg_ctl_t), 0,
                                 mlc_chain_mask_session_write);
    if (chain == NULL)
    {
        return -1;
    } 

    mlc_pkg_ctl_t *ctl = chain_to_ctl(chain);
    ctl->session.health = s->health;
    mlc_pkg_body_ctl_t *body = &ctl->body;
    body->control = mlc_control_heartbeat;
    body->index_op = s->index_op;
    body->index = s->index; 
    body->ping_start_us = ctl_client->ping_start_us;   
    int ret = connection_data_send(c, chain, MLC_PKG_TYPE_CTL);
    chain_release(chain,mlc_chain_mask_session_write);
    return ret;
}

static int session_client_send_heartbeat(session_t *s, connection_t *c)
{
    cycle_t *cycle = s->cycle;
    chain_t *chain = chain_alloc(cycle->pool, sizeof(mlc_pkg_ctl_t), 0,
                                 mlc_chain_mask_session_write);
    if (chain == NULL)
    {
        return -1;
    } 

    mlc_pkg_ctl_t *ctl = chain_to_ctl(chain);
    ctl->session.health = s->health;
    mlc_pkg_body_ctl_t *body = &ctl->body;
    body->control = mlc_control_heartbeat;
    body->ping_start_us = cycle->time_now;
    body->ping_us = c->ping_us;
    body->index = s->index;
    body->index_op = s->index_op;

    int ret = connection_data_send(c, chain, MLC_PKG_TYPE_CTL);
    chain_release(chain, mlc_chain_mask_session_write);
    return ret;
}

session_t *session_get_by_si(cycle_t *cycle,sinfo_t *si)
{
    if(si->s)
    {
        return si->s;
    }
    session_t *s = tpool_get_free(cycle->sm);
    if (s == NULL)
    {
        return NULL;
    }    
    s->match = si->match;
    si->s = s;
    assert(s && s->active == 0);
    s->active = 1;
    s->start_time_ms = cycle->time_now_ms;
    session_change_state(s, mlc_state_connecting, __LINE__);

    event_timer_start(s->state_update, cycle->conf.heartbeat_check_ms, 1);

    s->sl = cycle->sl;
    s->on_status = cycle->sl->on_status;
    s->on_receive = cycle->sl->on_receive;
    return s;
}

static int session_on_connect_req(session_t *s, connection_t *c, mlc_pkg_ctl_t *ctl)
{   
    assert(c && ctl); //s can be null
    cycle_t *cycle = c->cycle;
    assert(cycle && cycle->sl);
    if (cycle->is_server <= 0)
    {
        TRACE_SESSION(s, "client recv connect pkg, error");
        //TODO:收到错误包时候的处理
        return 0;
    }
    if (s != NULL)
    {
        if (s->state != mlc_state_connecting)
        {
            TRACE_SESSION(s, "recv connect req, ignore");
            //TODO:收到错误包时候的处理
        }
        else
        {
            TRACE_SESSION(s, "recv repeated connect req");
            if(c->match == 0)
            {
                c->match = s->match;
            }
            session_send_ctl(s, c, mlc_control_connect_rsp);
        }
    }
    else
    {
        sinfo_t *si = sinfo_create(cycle->sc);
        if(si==NULL)
        {
            TRACE_SESSION(s,"sinfo create failed!!");
            return -2;
        }
        c->match = si->match;
        log_info(NULL,"session on connect_req c=%d match=%x",c->index,si->match);
        session_send_ctl(NULL, c, mlc_control_connect_rsp);
    }

    return 0;
}

static int session_on_connect_rsp(session_t *s, connection_t *c, mlc_pkg_ctl_t *ctl)
{
    cycle_t *cycle = c->cycle;
    if (cycle->is_server)
    {
        TRACE_SESSION(s,"server recv connect rsp ctl, error");
        //TODO:收到错误包时候的处理
        return 0;
    }

    if (s == NULL) 
    {
        TRACE_SESSION(s, "session on connect rsp, session is null");
        //session_send_ctl(s,c,mlc_control_close_req);
        connection_close(c, MLC_NULL_SESSION);
        return 0;
    }

    if (s->state == mlc_state_connecting)
    {
        uint32_t match = ctl->pkg.match;
        s->match = match;
        c->match = match;
        s->index_op = ctl->body.index;
        session_change_state(s, mlc_state_connected, __LINE__);
        session_client_send_heartbeat(s, c);
        session_active(s);
        TRACE_SESSION(s, "session on connect rsp, c=%d", c->index);
    }
    else
    {
        TRACE_SESSION(s, "session on connect rsp, but state[%d] error", s->state);
    }
    return 0;
}

static int session_on_attach_req(session_t *s, connection_t *c, mlc_pkg_ctl_t *ctl)
{
    cycle_t *cycle = c->cycle;
    if (cycle->is_server <= 0)
    {
        TRACE_SESSION(s, "client recv connect req ctl, error");
        //TODO:收到错误包时候的处理
        return 0;
    }
    mlc_pkg_body_ctl_t *body = &ctl->body;
    if (s != NULL) 
    {
        assert(body->index_op == s->index);
        //TRACE_SESSION(s, "session on connect rsp, session is null");
        //session_send_ctl(s,c,mlc_control_close_req);
        //connection_close(c, MLC_NULL_SESSION);
        return 0;
    }

    s = (session_t*)tpool_get_by_index(cycle->sm, body->index_op);
    if (s == NULL)
    {
        log_warn(NULL,"session on attach req, but session not found, c=%d, s=%d", c->index, body->index_op);
        return -1;
    }
    //TODO:鉴权

    uint8_t line = body->line;
    if (line >= MAX_SESSION_CONNECTIONS)
    {
        log_warn(NULL,"session on attach req, but line out of range, c=%d, s=%d, line=%d", c->index, body->index_op, line);
        return -1;
    }

    //connection_t *c = s->connections[line];

    return 0;
}

static int session_on_attach_rsp(session_t *s, connection_t *c, mlc_pkg_ctl_t *ctl)
{
    cycle_t *cycle = c->cycle;
    if (cycle->is_server)
    {
        TRACE_SESSION(s, "server recv attach rsp ctl, error");
        //TODO:收到错误包时候的处理
        return 0;
    }

    if (s == NULL) 
    {
        TRACE_SESSION(s, "session on connect rsp, session is null");
        //session_send_ctl(s,c,mlc_control_close_req);
        connection_close(c, MLC_NULL_SESSION);
        return 0;
    }

    return 0;
}

static int session_on_heartbeat(session_t *s, connection_t *c, mlc_pkg_ctl_t *ctl)
{
    if (s == NULL) 
    {
        log_warn(NULL, "session on heartbeat, session is null");
        //session_send_ctl(NULL, c, mlc_control_close_req);;
        connection_close(c, MLC_NULL_SESSION);
        return -1;
    }

    if (s->state != mlc_state_connecting && s->state != mlc_state_connected)
    {
        TRACE_SESSION(s, "session on heartbeat, but state[%d] error", s->state);
        //session_send_ctl(s, c, mlc_control_close_req);;
        //connection_close(c, MLC_SESSION_STATE_ERROR);
        return -1;
    }    
    mlc_pkg_body_ctl_t *body = &ctl->body;
    cycle_t *cycle = c->cycle;
    if (cycle->is_server)
    {
        c->ping_us = body->ping_us;
        session_server_send_heartbeat(s, c, body);

        //握手成功后，第一次收到客户端的包(包括心跳包)时，将状态置为connected
        if (s->state == mlc_state_connecting)
        {
            if(s->sl->on_connected)
            {
                s->sl->on_connected(s, s->sl->data);
            }
            session_change_state(s, mlc_state_connected, __LINE__);
            session_active(s);
        }
    }
    else
    {
        c->ping_us = cycle->time_now - body->ping_start_us;
    }

    if(c->ping_us > 300 * 1000)
    {
        TRACE_SESSION(s, "connect[%d] on heartbeat, ping=%lu, ping_start=%lu, cycle_now=%lu", c->index, c->ping_us,body->ping_start_us,cycle->time_now);
    }

    return 0;
}

static int session_on_close_req(session_t *s, connection_t *c, mlc_pkg_ctl_t *ctl)
{
    if (s == NULL) 
    {
        log_warn(NULL, "session on close req, session is null");
        //session_send_ctl(NULL, c, mlc_control_close_req);;
        connection_close(c, MLC_NULL_SESSION);
        return -1;
    }

    //TODO:这里的处理似乎有问题
    if(s->state == mlc_state_close_wait)
    {
        session_send_ctl(s, c, mlc_control_close_rsp);
        session_stop(s, 1);
    }
    else
    {
        session_graceful_close(s, 2);
    }

    return 0;
}

static int session_on_close_rsp(session_t *s, connection_t *c, mlc_pkg_ctl_t *ctl)
{
    if (s == NULL)
    {
        log_warn(NULL, "session on close rsp, session is null");
        //session_send_ctl(NULL, c, mlc_control_close_req);;
        connection_close(c, MLC_NULL_SESSION);
        return -1; 
    }

    if (s->state == mlc_state_close_wait)
    {
        session_close(s, 0);
    }
    else
    {
        TRACE_SESSION(s, "session on close rsp, session state[%d] error", s->state);
        //connection_close(c, MLC_SESSION_STATE_ERROR);   
    }
    return 0;
}

static int session_on_control_recv(session_t *s, connection_t *c, chain_t *chain)
{
    int ret = -1;
    mlc_pkg_ctl_t *ctl = chain_to_ctl(chain);
    mlc_pkg_body_ctl_t *body = &ctl->body;
    switch (body->control)
    {
        case mlc_control_connect_req:
            ret = session_on_connect_req(s, c, ctl);
            break;
        case mlc_control_connect_rsp:
            ret = session_on_connect_rsp(s, c, ctl);
            break;
        case mlc_control_close_req:
            ret = session_on_close_req(s, c, ctl);
            break;
        case mlc_control_close_rsp:
            ret = session_on_close_rsp(s, c, ctl);
            break;
        case mlc_control_attach_req:
            ret = session_on_attach_req(s, c, ctl);
            break;
        case mlc_control_attach_rsp:
            ret = session_on_attach_rsp(s, c, ctl);
            break;
        case mlc_control_heartbeat:
            ret = session_on_heartbeat(s, c, ctl);
            break;
        default:
            log_warn(NULL, "unknow connection ctl[%d] pkg", body->control);
            //connection_close(c, MLC_UNKNOW_CONNECTION_CTL_TYPE);
            break;
    } 
    return ret;
}

static int session_on_data_recv(session_t *s, connection_t *c, chain_t *chain)
{
    if (s == NULL) 
    {
        log_warn(NULL, "session on data recv, session is null");
        //session_send_ctl(NULL,c,mlc_control_close_res);
        connection_close(c, MLC_NULL_SESSION);
        return -1;
    }
    
    if (s->state != mlc_state_connecting && s->state != mlc_state_connected)
    {
        TRACE_SESSION(s, "session on data recv, session state[%d] error", s->state);
        //session_send_ctl(s,c,mlc_control_close_res);
        //connection_close(c, MLC_SESSION_STATE_ERROR);
        return -1;
    }    

    cycle_t *cycle = s->cycle;
    assert(cycle);

    if (cycle->is_server && s->state == mlc_state_connecting)
    {
        session_change_state(s, mlc_state_connected, __LINE__);
        s->sl->on_connected(s, s->sl->data);
        session_active(s);
    }

    int ret = fec_input(s->fec, chain);
    if (ret < 0)
    {
        TRACE_SESSION(s, "fec input err ret=%d", ret);
    }

    return ret;
}

static inline int session_check_pkg(mlc_pkg_t *pkg)
{
    return pkg->line < MAX_SESSION_CONNECTIONS;
}

void session_attach_connection(session_t *s,connection_t *c,uint8_t line)
{
    connection_t *c1 = s->connections[line];
    TRACE_SESSION(s,"attach c=%d match=%x",c->index,s->match);
    if(c1)
    {
        connection_close(c1,MLC_RECONNECT);
    }
    c->userdata = s;
    s->connections[line] = c;
    c->match = s->match; 
    if(s->state == mlc_state_disconnect)
    {
        if(c->state == mlc_state_connecting || c->state == mlc_state_connected)
        {
            TRACE_SESSION(s,"live_by_reconnect c=%d cs=%d",c->index,c->state);
            session_change_state(s, mlc_state_connected, __LINE__);
        }
    }
}

int session_on_connection_status_c(connection_t *c,uint32_t state)
{
    if(state == mlc_state_disconnect)
    {
        cycle_t *cycle = c->cycle;
        uint64_t difms = cycle->time_now_ms - c->start_time_ms ;
        if( difms > 10)
        {
            session_t *s = c->userdata;
            TRACE_SESSION(s,"fast reconnect c=%d difms=%ld",c->index,difms);
            connection_reconnect(c);
        }
    }
    return 0;
}

void session_set_userdata(session_t *s, void *data)
{
	if (s != NULL) s->userdata = data;
}

void *session_get_userdata(session_t *s)
{
	return s != NULL ? s->userdata : NULL;
}


int session_on_connection_data(connection_t *c, session_t *s, chain_t *chain)
{
    mlc_pkg_t *pkg = chain_to_pkg(chain);
    assert(c && "connect must exist");
    cycle_t *cycle = c->cycle;
    if(session_check_pkg(pkg)<0)
    {
        pkg_debug(chain,"invalid pkg");
        return -1;
    }
    if(pkg->match)
    {
        if(c->match && c->match != pkg->match)
        {
            log_warn(NULL, "mismatch c=%d match=%x pkgmatch=%x", c->index,
                     c->match, pkg->match);
            return -2;
        }
        if(s)
        {
            if(s->match && s->match != pkg->match)
            {
                TRACE_SESSION(s, "mismatch smatch=%x pkgmatch=%x",
                    s->match,pkg->match);
                return -2;      
            }
        }
        else
        {
            sinfo_t *si = sinfo_find(cycle->sc,pkg->match);
            if(!si)
            {
                log_warn(NULL,"cannot find match=%x,send close_res",pkg->match);
                session_send_ctl(NULL,c,mlc_control_close_rsp);
                connection_close(c,MLC_NULL_SESSION);
                return -3;
            }
            s = si->s;
            if(s==NULL)
            {
                s = session_get_by_si(cycle,si);
                if (s == NULL)
                {
                    log_warn(NULL, "create sesssion failed");
                    return MLC_SESSION_FULL;
                }
            }
            session_attach_connection(s,c,pkg->line);
        }
    }
    c->index_op = pkg->index;
    if (pkg->type == MLC_PKG_TYPE_CTL)
    {
        if(s && !s->index_op)
        {
            s->index_op = chain_to_ctl(chain)->body.index;
        }
        return session_on_control_recv(s, c, chain);
    }
    else if (pkg->type == MLC_PKG_TYPE_DATA)
    {
        return session_on_data_recv(s, c, chain);
    }
    else
    {
        TRACE_SESSION(s, "unknow connection pkg type[%d]", pkg->type);
        connection_close(c, MLC_UNKNOW_CONNECTION_PKG_TYPE);
        return -1;
    }
}


connection_t *find_connection(session_t *s,uint8_t state)
{
    connection_t *ret = NULL;
    for (int i = 0; i < MAX_SESSION_CONNECTIONS; i++)
    {
        connection_t *c = s->connections[i];
        if (c == NULL) 
        {
            break;
        }
        if(c->state == state){
            ret = c;
            break;
        }
    }
    return ret;
}

static int state_update_on_connecting_s(session_t *s)
{
    assert(s->active);
    cycle_t *cycle = s->cycle;

    connection_t *c = s->connections[0];
    assert(c);
    if (c->state != mlc_state_connected)
    {
        TRACE_SESSION(s, "c=%d connecting failed", c->index);
        session_change_state(s, mlc_state_lost, __LINE__);
        return 0;
    }

    int64_t now_ms = (int64_t)cycle->time_now_ms;
    uint32_t timeout = cycle->conf.heartbeat_timeout_ms;
    int64_t dif = now_ms - s->start_time_ms;
    if (dif >= timeout || dif < 0)
    {
        TRACE_SESSION(s, "c=%d connecting timeout, diff=%l", c->index, dif);
        session_change_state(s, mlc_state_lost, __LINE__);
        return 0;
    }

    session_send_ctl(s, c, mlc_control_connect_rsp);
    return 0;
}

static int state_update_on_connected_s(session_t *s)
{
    assert(s->active);
    cycle_t *cycle = s->cycle;

    int64_t now_ms = (int64_t)cycle->time_now_ms;
    uint32_t timeout = cycle->conf.heartbeat_timeout_ms;

    int c_active = 0;
    for (int i = 0; i < MAX_SESSION_CONNECTIONS; i++)
    {
        connection_t *c = s->connections[i];
        if (c == NULL)
        {
            continue;
        }

        if (c->state == mlc_state_connected)
        {
            int64_t dif = now_ms - c->last_active_ms;
            if (dif >= timeout || dif < 0) 
            {
                TRACE_SESSION(s, "no heart beat too long dif=%lld",dif);
                connection_close(c, MLC_HEARTBEAT_TIMEOUT);
                s->connections[i] = NULL;
            }
            else
            {
                ++c_active;
            }
        }
        else if (c->state == mlc_state_disconnect)
        {
            connection_close(c, MLC_HEARTBEAT_TIMEOUT);
            s->connections[i] = NULL;
        }
    }

    if (c_active == 0)
    {
        TRACE_SESSION(s, "no heart beat too long");
        session_change_state(s, mlc_state_disconnect, __LINE__);
    }

    return 0;
}

static int state_update_on_disconnected_s(session_t *s){
    assert(s->active);
    cycle_t *cycle = s->cycle;

    uint64_t now_ms = cycle->time_now_ms;
    uint64_t last_active = 0;
    for (int i = 0; i < MAX_SESSION_CONNECTIONS; i++)
    {
        connection_t *c = s->connections[i];
        if (c == NULL)
        {
            continue;
        }
        if(c->last_active_ms > last_active){
            last_active = c->last_active_ms;
        }
    }
    uint64_t dif = now_ms - last_active;
    if (dif >= cycle->conf.lost_timeout_ms && now_ms > last_active){
        WARN_SESSION(s, "disconnect too long dif=%llu", dif);
        session_change_state(s,mlc_state_lost,__LINE__);
    }

    return 0;
}

static int state_update_on_wait_close(session_t *s)
{
    int c_active = 0;
    for (int i = 0; i < MAX_SESSION_CONNECTIONS; i++)
    {
        connection_t *c = s->connections[i];
        if (c == NULL)
        {
            continue;
        }
        
        if (c->state == mlc_state_connected)
        {
            session_send_ctl(s, c, mlc_control_close_req);
            connection_close(c, MLC_SESSION_CLOSE);
            s->connections[i] = NULL;
            ++c_active;
        }
    }
    
    if (c_active == 0)
    {
        session_close(s, MLC_SESSION_CLOSE);
    }

    return 0;
}

static int state_update_on_connecting_c(session_t *s)
{
    assert(s->active);
    cycle_t *cycle = s->cycle;
    connection_t *c = s->connections[0];
    assert(c);

    if ((cycle->time_now_ms - s->start_time_ms) > cycle->conf.connect_timeout_ms)
    {
        TRACE_SESSION(s, "c=%d, connect to [%s:%d] timeout", c->index, s->conf.ip, s->conf.port);
        session_change_state(s, mlc_state_lost, __LINE__);
        return 0;
    }

    int ret = connection_reconnect(c);
    if (ret < 0)
    {
        TRACE_SESSION(s, "c=%d connect [%s:%d] failed! need retry", c->index, s->conf.ip, s->conf.port);
    }

    if (c->state == mlc_state_connected)
    {
        session_send_ctl(s, c, mlc_control_connect_req);
    }

    if (c->state != mlc_state_connecting && c->state != mlc_state_connected)
    {
        TRACE_SESSION(s, "c=%d connect to [%s:%d] failed, state[%d] error!!!!!", c->index, s->conf.ip, s->conf.port, c->state);
        session_change_state(s, mlc_state_lost, __LINE__);
        return 0;
    }
    return 0;
}

static int state_update_on_connected_c(session_t *s)
{
    cycle_t *cycle = s->cycle;
    int64_t now_ms = (int64_t)cycle->time_now_ms;
    uint32_t timeout = cycle->conf.heartbeat_timeout_ms;

    int c_active = 0;
    for (int i = 0; i < MAX_SESSION_CONNECTIONS; i++)
    {
        connection_t *c = s->connections[i];
        if (c == NULL)
        {
            continue;
        }

        if (c->state == mlc_state_connected)
        {
            session_client_send_heartbeat(s, c);
            ++c_active;
        }
        else if (c->state == mlc_state_disconnect)
        {
            connection_reconnect(c);
            ++c_active;
        }
        else if(c->state == mlc_state_connecting)
        {
            ++c_active;   
        }
        
    }

    if (c_active == 0)
    {
        TRACE_SESSION(s, "no heart beat too long");
        session_change_state(s, mlc_state_disconnect, __LINE__);
    }

    return 0;
}

static int session_state_update(session_t *s)
{
    assert(s->active);
    cycle_t *cycle = s->cycle;
    state_obj_t *so = (cycle->is_server ? state_table_s : state_table_c) + s->state;
    if (so->on_update)
    {
        so->on_update(s);
    }
    return 0;
}

static int session_choose_line(session_t *s)
{
    for (int i = 0; i < MAX_SESSION_CONNECTIONS; i++)
    {
        connection_t *c = s->connections[i];
        if (c && c->state ==  mlc_state_connected) 
        {
            return i;
        }
    }
    return -1;
}

int session_listener_set_handler(session_listener_t *sl, void *data, session_on_connected_handler on_connected, session_on_receive_handler on_receive, session_on_status_change_handler on_status)
{
    sl->data = data;
    sl->on_connected = on_connected;
    sl->on_receive = on_receive;
    sl->on_status = on_status;
    return 0;
}

int session_set_receive_function(session_t *s, session_on_receive_handler function)
{
    s->on_receive = function;
    return 0;
}

int session_set_status_change_function(session_t *s, session_on_status_change_handler function)
{
    s->on_status = function;
    return 0;
}

session_listener_t *session_listener_create(cycle_t *cycle, const mlc_addr_conf_t *addr)
{
    session_listener_t *sl = malloc(sizeof(session_listener_t));
    sl->cycle = cycle;
    cycle->sl = sl;
    sl->udp_listener = create_udp_listener(cycle, addr, sl);
    sl->tcp_listener = create_tcp_listener(cycle, addr, sl);
    return sl;
}

void session_active(session_t *s)
{
    if (s->timer_update->started == 0) 
    {
        event_timer_start(s->timer_update, 0, 0);
    }
}

int session_data_send(session_t *s, const char *data, uint32_t data_len)
{
    if(s->state >= mlc_state_lost){
        return MLC_SESSION_STATE_ERROR;
    }
    if (data == NULL || data_len == 0) 
    {
        return 0;
    }

    if( data_len > MLC_DATA_MAX_SIZE)
    {
        return MLC_DATA_TOO_LARGE;
    }

    if (s->send_busy)
    {
        TRACE_SESSION(s,"send_but_busy len=%d",data_len);
    }
    if(s->write_queue->size >= s->write_queue->max){
        return MLC_SESSION_WRITE_QUEUE_FULL;
    }

    session_buffer_node_t *p = data_buffer_alloc(
        s->write_buffer, data_len + sizeof(session_buffer_node_t));
    if(p==NULL){
        return MLC_SESSION_WRITE_BUFFER_FULL;
    }
    p->len = data_len;
    memcpy(p->data,data,data_len);
    dqueue_ptr_push_back(s->write_queue, p);

    session_active(s);
    return 0;
}

session_t *session_create_client(cycle_t *cycle, const mlc_addr_conf_t *conf)
{
    session_t *s = tpool_get_free(cycle->sm);
    assert(s && s->active == 0);
    if (s == NULL)
    {
        return NULL;
    }
    s->active = 1;

    memcpy(&s->conf, conf, sizeof(mlc_addr_conf_t));
    return s;
}

session_t *session_create_client_by_url(cycle_t *cycle, const char *url)
{
    mlc_addr_conf_t conf;
    mlc_url_to_conf(url,&conf);
    session_t *s = session_create_client(cycle,&conf);
    return s;
}


int session_connect(session_t *s)
{
    cycle_t *cycle = s->cycle;
    s->state = mlc_state_connecting;
    //TODO add more connection by config here
    connection_t *c = connection_create(cycle, s, s->conf.ip, s->conf.port, MLC_CONNECT_MODE_UDP);
    if (c)
    {   
        c->on_status = (connection_on_status_handler)session_on_connection_status_c;
        s->connections[0] = c;
        connection_set_read_function(c, (connection_on_read_handler)session_on_connection_data);
        int ret = connection_reconnect(c);
        if (ret >= 0)
        {
            session_send_ctl(s, c, mlc_control_connect_req);
        }
        else
        {
            TRACE_SESSION(s, "c=%d connect [%s:%d] failed! need retry", c->index, s->conf.ip, s->conf.port);
        }
    }
    else
    {
        TRACE_SESSION(s, "connect [%s:%d] failed! connection create failed", s->index, s->conf.ip, s->conf.port);
        return -1;
    }

    s->start_time_ms = cycle->time_now_ms;
    event_timer_start(s->state_update, cycle->conf.heartbeat_check_ms, 1);
    TRACE_SESSION(s,"client start connecting server [%s:%d]", s->conf.ip, s->conf.port);
    return 0;
}

int session_getinfo(session_t * s, session_info_t* info)
{
	if (info != NULL)
	{
		info->health = s->health;
		info->state = s->state;
		info->active = s->active;
		info->send_busy = s->send_busy;		
	}

	return 0;
}

int session_flush_write(session_t *s)
{
    cycle_t *cycle = s->cycle;
    int sent = 0;

    while(dequeue_ptr_size(s->write_queue) > 0){
        session_buffer_node_t *node = dqueue_ptr_front(s->write_queue);
        uint32_t data_len = node->len;
        uint8_t count =
            data_len <= s->mss ? 1 : (data_len + s->mss - 1) / s->mss;
        chain_t *chain_array[MLC_MAX_FRAGMENT_CNT];
        cycle_t *cycle = s->cycle;

        if (s->enable_kcp) {
            if (count > kcp_send_wnd_left(s->kcp)) {
                session_active(s);
                s->send_busy = 1;
                return MLC_BUSY;
            }
        }

        for (int i = 0; i < count; i++) {
            chain_t *chain = chain_alloc(cycle->pool, 0, MLC_DEFAULT_MTU,
                                         mlc_chain_mask_session_write);
            if (chain == NULL) {
                for (int j = 0; j < i; j++) {
                    chain_t *c = chain_array[j];
                    chain_release_one(c, mlc_chain_mask_session_write);
                }
                session_active(s);
                return MLC_CHAIN_ALLOC_FAILED;
            }
            chain_array[i] = chain;
        }

        uint8_t *data = node->data;
        for (int i = 0; i < count; i++) {
            uint32_t size = data_len > s->mss ? s->mss : data_len;
            chain_t *chain = chain_array[i];
            chain->len = chain_len_from_session(size);
            mlc_pkg_session_t *p = chain_to_session(chain);
            p->sn = ++s->snd_nxt;
            p->frg = count - i - 1;
            chain_to_pkg(chain)->type = MLC_PKG_TYPE_DATA;
            memcpy(p->data, data, size);
            data = (uint8_t *)data + size;
            data_len -= size;
            sent += 1;
            int ret = kcp_send(s->kcp, chain, s->enable_kcp);
            if (ret >= 0) {
                chain_release_one(chain, mlc_chain_mask_session_write);
            } else {
                WARN_SESSION(s, "kcp send ret=%d busy_now, cannot run here", ret);
            }
        }
        dequeue_ptr_pop_front(s->write_queue);
    }

    if(sent)
    {
        session_active(s);
        data_buffer_reset(s->write_buffer);
    }

    return sent;
}

int session_update(session_t * s)
{
    cycle_t *cycle = s->cycle;
    //如果connected 才进行kcp send ,但是其他情况也要kcp update
    switch(s->state){
        case mlc_state_connected:
        {
            session_flush_write(s);
        }
        break;
        case mlc_state_lost:
        case mlc_state_disconnect:
            return 0;
        default:
            break;
    }

    uint32_t kcp_ms = cycle->time_now_ms - s->start_time_ms;
    uint32_t next_update = kcp_update(s->kcp, kcp_ms);
    fec_flush(s->fec);
    if (next_update == kcp_ms)
    {
        session_active(s);
    }
    else if (next_update > kcp_ms)
    {
        uint32_t delay = next_update - kcp_ms;
        if (s->timer_update_delay->started) 
        {
            event_timer_stop(s->timer_update_delay);
        }
        event_timer_start(s->timer_update_delay, delay, 0);
    }
    if (s->send_busy)
    {
        if(kcp_send_wnd_left(s->kcp) > 0)
        {
            TRACE_SESSION(s,"session_not_busy_now");
            s->send_busy = 0;
        }
    }

    return 0;
}

int session_recv(session_t *s, char *data, int data_len)
{
    chain_t *chain = s->read_chain;
    if (chain == NULL)
    {
        return 0;
    }

    mlc_pkg_session_t *sdata = &chain_to_data(chain)->session;
    if (s->read_chain_n <= sdata->frg)
    {
        return 0;
    }

    int size = 0;
    for (int i = sdata->frg; i>=0; i--)
    {
        mlc_pkg_data_t *pkg = chain_to_data(chain);
        sdata = &pkg->session;
        assert(i == sdata->frg);
        int len = chain_to_session_len(chain);
        assert(len >= 0);
        size += len;
        assert(size <= data_len);
        if (size > data_len)
        {
            return -1;
        }
        memcpy(data, sdata->data, len);
        data += len;
        chain = chain->next;
    }

    while (s->read_chain != chain)
    {
        s->read_chain = chain_release_one(s->read_chain, mlc_chain_mask_session_read);
        s->read_chain_n -= 1;
    }

    return size;
}
