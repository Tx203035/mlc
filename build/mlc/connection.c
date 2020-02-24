#include "connection.h"
#include "event.h"
#include "core/log.h"
#include "cycle.h"
#include "core/palloc.h"
#include "core/socket_util.h"
#include "core/chain.h"
#include "core/tpool.h"
#include "session.h"
#include "core/encoding.h"
#include "kcp.h"
#include "fec.h"
#include "sinfo.h"

#define TRACE_CONN(c,fmt,...) log_info(L_CON(c->cycle),"[conn]c=%d c_op=%d p=%p match=%x state=%d fd=%d|"fmt,\
    c->index, c->index_op, c , c->match ,c->state,c->fd, ##__VA_ARGS__)
#define TRACE_CONN_ERRNO(c,fmt,...) log_errno(L_CON(c->cycle),"[conn]c=%d c_op=%d p=%p match=%x state=%d fd=%d|"fmt,\
    c->index, c->index_op, c ,c->match ,c->state,c->fd, ##__VA_ARGS__)

static int connection_on_read(connection_t *conn);
static int connection_on_write(connection_t *conn);
static int connection_handler_chain(connection_t *conn);
static int connection_read_temp_chain(connection_t *conn);

static inline void connection_close_fd(connection_t *c,int reason)
{
    assert(c->fd>0);
    TRACE_CONN(c,"closefd reason=%d",reason);
    mlc_io_mult_del_event(c->cycle, c->event);
    int ret = mlc_close(c->fd);
    if(ret < 0)
    {
        TRACE_CONN_ERRNO(c,"close fd failed");
    }
    c->fd = 0;
    c->event->fd = 0;
}

static void connection_change_state(connection_t *conn, uint32_t state,int line)
{
    TRACE_CONN(conn,"state change %d=>%d line=%d", conn->state, state, line);
    conn->state = state;
    if (conn->on_status) 
    {
        conn->on_status(conn, state);
    }
}

int connection_listener_set_handler(connection_listener_t *cl, connection_on_accept_handler on_accept)
{
    cl->on_accept = on_accept;
    return 0;
}

static int connection_tcp_accept(connection_listener_t *tl)
{
    struct sockaddr_in sock_addr = {0};
    socklen_t sock_len = sizeof(sock_addr);
    cycle_t *cycle = tl->cycle;
    int new_fd;
    while(1)
    {
ACCEPT_AGAIN:        
        new_fd = accept(tl->fd, (struct sockaddr *)&sock_addr, &sock_len);
        if (new_fd < 0)
        {
            int err = mlc_errno;
            if (err == MLC_EINTR)
            {
                goto ACCEPT_AGAIN;
            }            
            // log_error(NULL,"tcp listener accept failed");
            break;
        }
        int ret = socket_nonblocking(new_fd);
        if (ret < 0)
        {
            log_error(L_SYS(cycle),"set nonblock ret=%d", ret);
            mlc_close(new_fd);
            return -1;
        }

        connection_t *conn = tpool_get_free(cycle->cm);
        if (!conn)
        {
            log_info(L_SYS(cycle),"connection pool full!!");
            return -1;
        }
        conn->active = 1;
        conn->state = mlc_state_connecting; //tcp accept as connected
        conn->last_active_ms = cycle->time_now_ms;
        conn->mode = MLC_CONNECT_MODE_TCP;
        conn->fd = new_fd;
        event_t *event = conn->event;
        event->fd = new_fd;

        mlc_io_mult_add_event(cycle, event, MLC_EVENT_READ);

        connection_change_state(conn, mlc_state_connected,__LINE__);
        conn->on_read = tl->on_read;
        conn->on_status = tl->on_status;

        if (tl->on_accept)
        {
            tl->on_accept(conn, tl->data);
        }
    }

    return 0;
}

//listen 也需要用到connection
connection_listener_t *create_tcp_listener(cycle_t *cycle, const mlc_addr_conf_t *addr, void *data)
{
    int fd = socket_tcp_create();
    if (fd < 0)
    {
        log_error(NULL,"create tcp socket failed");
        return NULL;
    }

    connection_listener_t *tl = malloc(sizeof(connection_listener_t));
    assert(tl);
    struct sockaddr_in *my_addr = &tl->socket_listen.sockaddr_in;
    memset(my_addr, 0, sizeof(*my_addr));
    my_addr->sin_family = PF_INET;
    my_addr->sin_port = htons(addr->port);
    my_addr->sin_addr.s_addr = inet_addr(addr->ip);
    if (bind(fd, (struct sockaddr *)my_addr, sizeof(struct sockaddr)) < 0)
    {
        log_errno(NULL,"bind fd=%d",fd);
        return NULL;
    }
    if (listen(fd, cycle->conf.backlog) < 0)
    {
        log_errno(NULL,"listen %d failed", fd);
        return NULL;
    }

    tl->cycle = cycle;
    tl->fd = fd;
    tl->data = data;
    tl->mode = MLC_CONNECT_MODE_TCP;
    tl->on_accept = NULL;
    tl->on_read = (connection_on_read_handler)session_on_connection_data;
    tl->on_status = NULL;

    event_t *evt = create_event(tl, (event_handler_pt)connection_tcp_accept, NULL);
    assert(evt);
    evt->fd = fd;
    tl->event = evt;

    int ret = mlc_io_mult_add_event(cycle, evt, MLC_EVENT_READ);
    if (ret < 0)
    {
        log_error(NULL,"add io mult for listener failed!,fd=%d", fd);
        return NULL;
    }
    log_warn(NULL,"tcp listen at [%s:%d], fd[%d]", addr->ip, addr->port, fd);
    return tl;
}

static int udp_send_ctl_to(connection_listener_t *ul,struct sockaddr * addr,socklen_t addr_len,uint8_t ctl_code,uint32_t match)
{
    cycle_t *cycle = ul->cycle;
    chain_t *chain = chain_alloc(cycle->pool, sizeof(mlc_pkg_ctl_t), 0,
                                 mlc_chain_mask_connection_write);
    if (chain == NULL)
    {
        return -1;
    }

    mlc_pkg_ctl_t *ctl = chain_to_ctl(chain);
    memset(ctl, 0, sizeof(mlc_pkg_ctl_t));
    ctl->body.control = ctl_code;
    mlc_pkg_t *pkg = &ctl->pkg;
    pkg->len = chain_to_connection_len(chain);
    pkg->type = MLC_PKG_TYPE_CTL;
    pkg->index = 0;
    pkg->match = match;
    pkg->line = 0;
    ++cycle->stat.sent;
    int ret = socket_send_to(ul->fd,chain->data_start,chain->len, addr,addr_len);
    chain_release(chain,mlc_chain_mask_connection_write);
    return ret;
}
static int connection_udp_accept(connection_listener_t *ul)
{
    struct sockaddr_in sock_addr = {0};
    socklen_t sock_len = sizeof(sock_addr);
    cycle_t *cycle = ul->cycle;
    while(1)
    {
        int ret = -1;
        int err = -1;
        if(ul->temp_chain == NULL)
        {
            ul->temp_chain = chain_alloc(ul->cycle->pool, 0, MLC_DEFAULT_MTU, mlc_chain_mask_connection_read);
        }
        chain_t *chain = ul->temp_chain;
RECV_AGAIN:
        ret = recvfrom(ul->fd, chain->data_start, MLC_DEFAULT_MTU, 0, (struct sockaddr *)&sock_addr, &sock_len);
        if (ret < 0)
        {
            err = mlc_errno;
            if (err == MLC_EINTR)
            {
                goto RECV_AGAIN;
            }
            else
            {
                return 0;
            }
        }
        chain->len = ret;
        mlc_pkg_t *pkg = chain_to_pkg(chain);
        connection_t *c = NULL;
        if (pkg->match)
        {
            if(pkg->line >= MAX_SESSION_CONNECTIONS)
            {
                log_error(NULL,"unknow line %d",pkg->line);
                continue;
            }
            sinfo_t *si = sinfo_find(cycle->sc,pkg->match);
            if(si==NULL || si->match != pkg->match)
            {
                log_error(NULL,"match=%x(%x) not found from ip=%s"
                    ,pkg->match,si ? si->match : 0,
                    inet_ntoa(sock_addr.sin_addr));
                continue;
            }
            session_t *s = si->s;
            if(s == NULL)
            {
                s = session_get_by_si(cycle,si);
                if(s==NULL)
                {
                    log_error(NULL, "create sesssion failed,ip=%s",
                              inet_ntoa(sock_addr.sin_addr));
                    continue;
                }
                si->s = s;
                s->match = si->match;
            }
            c = s->connections[pkg->line];
            if(c == NULL)
            {
                c = tpool_get_free(cycle->cm);
                if(c == NULL)
                {
                    log_info(NULL, "no more free connections used=%d ip=%s",
                             cycle->cm->used, inet_ntoa(sock_addr.sin_addr));
                    continue;
                }
                c->active = 1;
                c->state = mlc_state_connecting; // udp recv init as connected
                c->on_read = ul->on_read;
                c->on_status = ul->on_status;
                session_attach_connection(s,c,pkg->line);
            }
            c->last_active_ms = cycle->time_now_ms;
            c->index_op = pkg->index;
            if(c->fd <=0 || memcmp(&sock_addr,&c->peer_addr,sock_len))
            {
                int fd = socket_udp_accept(&ul->socket_listen.sockaddr,(struct sockaddr *)&sock_addr);
                if(c->fd > 0)
                {
                    connection_close_fd(c,MLC_REATTACH);
                }
                c->fd = fd;
                event_t *event = c->event;
                event->fd = fd;
                mlc_io_mult_add_event(cycle, event, MLC_EVENT_READ);
                memcpy(&c->peer_addr,&sock_addr,sock_len);
            }
            if(c->state != mlc_state_connected)
            {
                connection_change_state(c, mlc_state_connected,__LINE__);
            }
            TRACE_CONN(c,"udp reconnected form %s:%d",
                inet_ntoa(sock_addr.sin_addr), htons(sock_addr.sin_port));
            ul->temp_chain = NULL;
            c->read_chain = chain_append_one(c->read_chain, chain);
            connection_handler_chain(c);                
        }
        else
        {
            if( pkg->type != MLC_PKG_TYPE_CTL )
            {
                log_error(NULL, "drop data pkg without match,ip=%s",
                          inet_ntoa(sock_addr.sin_addr));
                continue;
            }
            mlc_pkg_ctl_t *ctl = chain_to_ctl(chain);
            uint8_t ctl_code = ctl->body.control;
            if (ctl_code != mlc_control_connect_req)
            {
                log_error(NULL, "drop ctl=%d pkg without match ip=%s", ctl_code,
                          inet_ntoa(sock_addr.sin_addr));
                continue;
            }

            sinfo_t *si = sinfo_create(cycle->sc);
            if(si==NULL)
            {
                log_error(NULL, "create si failed,ip=%s",
                          inet_ntoa(sock_addr.sin_addr));
                continue;
            }

            log_warn(NULL,"udp connect_rsp to %s:%d match=%x",
                inet_ntoa(sock_addr.sin_addr), htons(sock_addr.sin_port),si->match);

            udp_send_ctl_to(ul,(struct sockaddr *)&sock_addr,sock_len,mlc_control_connect_rsp,si->match);
        }

    }

    return 0;
}

connection_listener_t *create_udp_listener(cycle_t *cycle, const mlc_addr_conf_t *addr, void *data)
{
    int fd = socket_udp_create();
    if (fd < 0)
    {
        log_error(L_CON(cycle),"create tcp socket failed\n");
        return NULL;
    }

    struct sockaddr_in my_addr;
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = PF_INET;
    my_addr.sin_port = htons(addr->port);
    my_addr.sin_addr.s_addr = inet_addr(addr->ip);
    if (bind(fd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) < 0)
    {
        log_error(L_CON(cycle),"bind %d failed", fd);
        return NULL;
    }

    connection_listener_t *ul = malloc(sizeof(connection_listener_t));
    assert(ul);
    ul->cycle = cycle;
    ul->fd = fd;
    ul->data = data;
    ul->mode = MLC_CONNECT_MODE_UDP;
    ul->temp_chain = NULL;
    ul->on_accept = NULL;
    ul->on_read = (connection_on_read_handler)session_on_connection_data;
    ul->on_status = NULL;

    event_t *evt = create_event(ul, (event_handler_pt)connection_udp_accept, NULL);
    assert(evt);
    evt->fd = fd;
    ul->event = evt;
    memcpy(&ul->socket_listen.sockaddr, &my_addr, sizeof(my_addr));

    int ret = mlc_io_mult_add_event(cycle, evt, MLC_EVENT_READ);
    if (ret < 0)
    {
        log_error(NULL,"epoll add error fd=%d", fd);
        return NULL;
    }

    log_warn(NULL,"udp listen at [%s:%d], fd[%d]", addr->ip,addr->port, fd);
    return ul;
}

int connection_init(connection_t *conn, void *init_conf, int index)
{
    memset(conn, 0, sizeof(connection_t));
    conn->cycle = (cycle_t *)init_conf;
    conn->index = index;
    conn->match = 0;
    conn->event = create_event(conn, (event_handler_pt)connection_on_read, (event_handler_pt)connection_on_write);
    assert(conn->event);
    return 0;
}

int connection_reset(connection_t *conn)
{
    cycle_t *cycle = conn->cycle;
    if (conn->fd)
    {
        connection_close_fd(conn,MLC_RESET);
    }

    if (conn->read_chain)
    {
        chain_release(conn->read_chain, mlc_chain_mask_connection_read);
        conn->read_chain = NULL;
    }

    if (conn->write_chain)
    {
        chain_release(conn->write_chain, mlc_chain_mask_connection_write);
        conn->write_chain = NULL;
    }

    if (conn->temp_chain)
    {
        chain_release(conn->temp_chain, mlc_chain_mask_connection_read | mlc_chain_mask_connection_write);
        conn->temp_chain = NULL;
    }

    conn->state = mlc_state_init;
    conn->mode = 0;
    conn->last_active_ms = 0;
    conn->userdata = NULL;
    conn->session = NULL;
    conn->match = 0;
    conn->raw = 0;
    conn->write_len = 0;
    conn->active = 0;
    conn->on_status = NULL;
    conn->on_read = NULL;
    conn->ping_us = 0;
    conn->socklen = 0;
    conn->sockaddr = NULL;
    conn->reconnect_n = 0;
    conn->attached = 0;
    conn->match = 0;
    memset(&(conn->peer_addr), 0, sizeof(conn->peer_addr));

    return 0;
}

int connection_release(connection_t *conn)
{
    //TODO
    return 0;
}

int connection_close(connection_t *conn, int reason)
{   
    assert(conn->active);
    if (conn->state != mlc_state_closed)
    {
        cycle_t *cycle = conn->cycle;
        connection_change_state(conn, mlc_state_closed,__LINE__);
        TRACE_CONN(conn, "closed reason=%d", reason);
        tpool_release(cycle->cm, conn); 
    }
    else
    {
        TRACE_CONN(conn, "has been closed before, reason=%d, but has been closed", reason);
    }
    
    return 0;
}

connection_t *connection_create(cycle_t *cycle, void *data, const char *ip_addr, unsigned short port, int mode)
{
    connection_t *conn = tpool_get_free(cycle->cm);
    if (conn == NULL)
    {
        return NULL;
    }
    conn->active = 1;
    struct sockaddr_in *peer_addr = &conn->peer_addr.sockaddr_in;
    memset(peer_addr, 0, sizeof(struct sockaddr_in));
    peer_addr->sin_family = PF_INET;
    peer_addr->sin_port = htons(port);
    peer_addr->sin_addr.s_addr = inet_addr(ip_addr);
    conn->mode = mode;
    conn->userdata = data;
    conn->state = mlc_state_init;
    TRACE_CONN(conn, "connect to addr=%s:%d", ip_addr, port);

    return conn;
}

int connection_set_read_function(connection_t *conn, connection_on_read_handler on_read)
{
    conn->on_read = on_read;
    return 0;
}

int connection_set_status_function(connection_t *conn, connection_on_status_handler on_status)
{
    conn->on_status = on_status;
    return 0;
}

void connection_set_raw(connection_t *conn)
{
    conn->raw = 1;
    conn->index_op = -1;
}

int connection_reconnect(connection_t *conn)
{
    int last_fd = conn->fd;
    int fd;
    cycle_t *cycle = conn->cycle;
    conn->start_time_ms = cycle->time_now_ms;
    if (conn->mode == MLC_CONNECT_MODE_TCP)
    {
        fd = socket_tcp_create();
    }
    else if (conn->mode == MLC_CONNECT_MODE_UDP)
    {
        fd = socket_udp_create();
    }
    else
    {
        TRACE_CONN(conn, "connect [%s:%d] failed, mode[%d] error", 
        inet_ntoa(conn->peer_addr.sockaddr_in.sin_addr), ntohs(conn->peer_addr.sockaddr_in.sin_port), conn->mode);
        return -1;
    }

    if (fd < 0)
    {
        TRACE_CONN_ERRNO(conn, "connect [%s:%d] failed, create socket[%d] error",
        inet_ntoa(conn->peer_addr.sockaddr_in.sin_addr), ntohs(conn->peer_addr.sockaddr_in.sin_port), conn->mode);
        return -1;
    }

    if (conn->fd > 0) 
    {
        connection_close_fd(conn,MLC_RECONNECT);
        if(conn->state != mlc_state_disconnect)
        {
            connection_change_state(conn, mlc_state_disconnect,__LINE__);
        }
    }

    struct sockaddr *peer_addr = &conn->peer_addr.sockaddr;
    int ret = connect(fd, peer_addr, sizeof(struct sockaddr_in));
    int inprogress = 0;
    if (ret != 0)
    {
        //TO DEAL ERROR
        int err = mlc_errno;
        if (err != MLC_EINPROGRESS && err != MLC_EWOULDBLOCK)
        {
            TRACE_CONN_ERRNO(conn, "connect ret=%d", ret);
            return -1;
        }
        else
        {
            inprogress = 1;
        }
    }

    conn->reconnect_n++;
    conn->fd = fd;
    conn->event->fd = fd;
    unsigned flags = inprogress ? MLC_EVENT_READWRITE : MLC_EVENT_READ;
    ret = mlc_io_mult_add_event(conn->cycle, conn->event, flags);
    if (ret < 0)
    {
        log_error(NULL,"c=%d connect [%s:%d] failed, epoll add fd[%d] error, ret[%d]", conn->index, 
        inet_ntoa(conn->peer_addr.sockaddr_in.sin_addr), ntohs(conn->peer_addr.sockaddr_in.sin_port), conn->fd, ret);
        return -1;
    }

    TRACE_CONN(conn, "connect to [%s:%d] fd[%d=>%d]", inet_ntoa(conn->peer_addr.sockaddr_in.sin_addr), ntohs(conn->peer_addr.sockaddr_in.sin_port), last_fd, fd);

    connection_change_state(conn, inprogress ? mlc_state_connecting : mlc_state_connected,__LINE__);
    return 0;
}

int connection_data_send(connection_t *conn, chain_t *chain, uint8_t type)
{
    chain_retain_one(chain, mlc_chain_mask_connection_write);
    if (conn->raw == 0) 
    {
        mlc_pkg_t *pkg = chain_to_pkg(chain);
        pkg->len = chain_to_connection_len(chain);
        pkg->type = type;
        pkg->index = conn->index;
        pkg->match = conn->match;
    }
    conn->write_chain = chain_append_one(conn->write_chain, chain);

    if(conn->state == mlc_state_connected)
    {
        connection_on_write(conn);
    }
    else
    {
        TRACE_CONN(conn, "connection_data_send, but state:%d error", conn->state);    
    }

    return 0;
}

static int connection_on_write(connection_t *conn)
{ 
    if(conn->fd <= 0)
    {
        return MLC_AGAIN;
    }
    if (conn->state == mlc_state_connecting)
    {
        connection_change_state(conn, mlc_state_connected,__LINE__);
    }

    if (!conn->write_chain)
    {
        return 0;
    } 

    cycle_t *cycle = conn->cycle;
    chain_t *chain = conn->write_chain;
    int ret = -1;
    int block = 0;
    while (chain)
    {
        if(conn->raw)
        {
        //    log_info(NULL,"connection c=%d fd=%d send raw pkg len=%d", conn->index, conn->fd, chain->len - conn->write_len);
        }
        else
        {
            //log_info(NULL,"conn[%d] send pkg", conn->index);
            //pkg_debug(chain, "connection send pkg");
        }
        int len = chain->len - conn->write_len;
        ret = socket_send(conn->fd, chain->data_start + conn->write_len, len);
        if (conn->mode == MLC_CONNECT_MODE_TCP){
            if (ret < 0) {
                TRACE_CONN_ERRNO(conn, "send error, ret=%d", ret);
                connection_close_fd(conn, MLC_DEBUG_CLOSE);
                connection_change_state(conn, mlc_state_disconnect, __LINE__);
                return ret;
            }
        }
        else{
            ret = len;
        }
        ++conn->sent_n;
        ++cycle->stat.sent;
        if (ret == 0)
        {
            block = 1;
            //log_info(NULL,"connection c=%d fd=%d send pkg break, need epoll mod", conn->index, conn->fd);
            break;
        }

        conn->write_len += ret;
        if (conn->write_len >= chain->len)
        {
            chain = chain_release_one(chain, mlc_chain_mask_connection_write);
            conn->write_chain = chain;
            conn->write_len = 0;
        }

        TRACE_CONN(conn,"connection write len=%d", ret);
    }

    if (block)
    {
        if(conn->event->flags != MLC_EVENT_READWRITE)
        {
            log_info(NULL,"connection c=%d fd=%d epoll mod flags=%d", conn->index, conn->fd, MLC_EVENT_READWRITE);
            mlc_io_mult_mod(cycle, conn->event, MLC_EVENT_READWRITE);
        }
    }
    else
    {
        if(conn->event->flags != MLC_EVENT_READ)
        {
            log_info(NULL,"connection c=%d fd=%d epoll mod flags=%d", conn->index, conn->fd, MLC_EVENT_READ);
            mlc_io_mult_mod(cycle, conn->event, MLC_EVENT_READ);
        }
    }
    // if(!conn->cycle->is_server && conn->sent_n % 100 == 20)
    // {
    //     log_info(NULL,"fake break connection");
    //     close(conn->fd);
    // }

    return 0;
}

int connection_read_temp_chain(connection_t *conn)
{
    chain_t *chain = conn->temp_chain;
    if (chain && chain->len > 0)
    {
        //TODO:TCP分包
        mlc_pkg_t *pkg = chain_to_pkg(chain);
        if (conn->raw || conn->mode == MLC_CONNECT_MODE_UDP)
        {
            // if(conn->raw){
            //     log_info(NULL,"connection recv raw pkg len=%d",chain->len);
            // }
            // else{
            //     pkg_debug(chain, "connection recv pkg");
            // }
            conn->read_chain = chain_append_one(conn->read_chain, chain);
            conn->temp_chain = NULL; 
        }
        else
        {
            //log_info(NULL,"conn[%d] read_temp_chain", conn->index);
            while (chain && chain->len > sizeof(uint16_t) && chain->len >= (pkg->len + sizeof(mlc_pkg_t)))
            {
                if (pkg->len == 0)
                {
                    //pkg_debug(chain, "connection recv error pkg");
                    assert(0);
                }
                conn->read_chain = chain_append_one(conn->read_chain, chain);
                //pkg_debug(chain, "connection recv pkg");

                chain = chain_split(chain, (pkg->len + sizeof(mlc_pkg_t)));
                if (chain)
                {
                    chain_retain_one(chain, mlc_chain_mask_connection_read);
                    pkg = chain_to_pkg(chain);
                }
                conn->temp_chain = chain;
            }
        }
    }
    return 0;
}


int connection_handler_chain(connection_t *conn)
{
    int ret = -1;
    while (conn->read_chain)
    {
        chain_t *chain = conn->read_chain;
        conn->read_chain = chain->next;
        chain->next = NULL;
        if (conn->on_read)
        {
            ret = conn->on_read(conn, conn->userdata, chain);
            if (ret < 0)
            {
                //TODO
                TRACE_CONN(conn, "chain:[%p] read handler err ret=%d", chain, ret);
                chain->next = conn->read_chain;
                conn->read_chain = chain;
                break;
            }
        }
        else
        {
            TRACE_CONN(conn, "drop no handler data len=%d", chain->len);
        }       
        chain_release_one(chain, mlc_chain_mask_connection_read);
    }

    return 0;
}

static int connection_on_read(connection_t *conn)
{
    if(conn->fd <= 0)
    {
        TRACE_CONN_ERRNO(conn, "err fd");
        return 0;
    }
    
    cycle_t *cycle = conn->cycle;
    conn->last_active_ms = cycle->time_now_ms;
    chain_t *chain = NULL;
    int ret = -1;
    for (;;)
    {
        if (conn->temp_chain == NULL)
        {
            conn->temp_chain = chain_alloc(conn->cycle->pool, 0, MLC_DEFAULT_MTU, mlc_chain_mask_connection_read);
        }
        chain = conn->temp_chain;
        int len_to_read = chain->capacity - chain->len;
        ret = socket_recv(conn->fd, chain->data_start + chain->len, len_to_read);
        if (ret < 0)
        {
            assert(conn->fd > 0);
            connection_close_fd(conn,MLC_RECV_ERR);
            if (ret == MLC_CONNECTION_CLOSE)
            {
                TRACE_CONN(conn, "peer close");
                ret = 0;
            }
            else
            {
                TRACE_CONN_ERRNO(conn, "recv error, ret=%d",ret);
            }
            connection_change_state(conn, mlc_state_disconnect,__LINE__);
            return ret;
        }
        else if (ret == 0)
        {
            // TRACE_CONN(conn,"break ret=0");
            break;
        }
        else
        {
            ++cycle->stat.recv;
            ++conn->recv_n;
            chain->len += ret;
            // log_info(NULL,"c=%d fd=%d fdevt=%d %p",conn->index,conn->fd,conn->event->fd, chain);
            connection_read_temp_chain(conn);
            TRACE_CONN(conn,"recv ret=%d to_read=%d",ret,len_to_read);
        }
    }
    connection_handler_chain(conn);
    return 0;
}
