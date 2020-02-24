#ifndef _LIBMLC_SESSION_H_
#define _LIBMLC_SESSION_H_

#include "mlc.h"

#define MAX_SESSION_CONNECTIONS 5

typedef int (*session_on_connected_handler)(session_t *s, void *data);
typedef int (*session_on_receive_handler)(session_t *s, const char *data, uint32_t size, void *userdata);
typedef int (*session_on_status_change_handler)(session_t *s, uint8_t state, void *userdata);

typedef struct {
    uint32_t len;
    uint8_t data[0];
}session_buffer_node_t;

struct session_s
{
    void *userdata;
    uint32_t match;
    cycle_t *cycle;
    session_on_receive_handler on_receive;
    session_on_status_change_handler on_status;
    mlc_addr_conf_t conf;
    event_timer_t *state_update;
    event_timer_t *timer_update;
    event_timer_t *timer_update_delay;
    int index;
    int index_op;
    int mss;
    kcp_pcb_t *kcp;
    fec_pcb_t *fec;
    dqueue_ptr_t *write_queue;
    data_buffer_t *write_buffer;
    chain_t *write_chain;
    uint32_t write_chain_n;
    chain_t *read_chain;
    uint32_t read_chain_n;
    chain_t *temp_chain;
    uint32_t temp_chain_n;
	uint8_t state;
    uint8_t health;
    uint32_t snd_nxt;
    uint32_t rcv_nxt;
    uint64_t start_time_ms;
    connection_t *connections[MAX_SESSION_CONNECTIONS];

    session_listener_t *sl;

    uint64_t next_kcp_update_ms;
    char recv_buffer[MLC_DATA_MAX_SIZE];

    unsigned active : 1;
    unsigned send_busy : 1;
    unsigned enable_fec : 1;
    unsigned enable_kcp : 1;
};

struct session_info_s
{
	uint8_t health;
	uint8_t state;
	uint8_t active;
	uint8_t send_busy;
	uint32_t ping;
};

struct session_server_conf_s
{
    char ip_addr[16];
    uint16_t port;
};

struct session_listener_s
{
    cycle_t *cycle;
    void *data;
    connection_listener_t *udp_listener;
    connection_listener_t *tcp_listener;
    session_on_connected_handler on_connected;
    session_on_receive_handler on_receive;
    session_on_status_change_handler on_status;
};


MLC_API int session_init(session_t *s, cycle_t *cycle, int index);
MLC_API int session_reset(session_t *s);
MLC_API int session_release(session_t *s);
MLC_API int session_close(session_t *s, int reason);

MLC_API void session_set_userdata(session_t *s, void *data);
MLC_API void *session_get_userdata(session_t *s);

MLC_API int session_on_connection_data(connection_t *c, session_t *s, chain_t *chain);
MLC_API int session_on_connection_accept(connection_t *c, session_listener_t *sl);

MLC_API session_t *session_create_client(cycle_t *cycle, const mlc_addr_conf_t *conf);
MLC_API session_t *session_create_client_by_url(cycle_t *cycle, const char *url);
MLC_API int session_connect(session_t *s);
MLC_API int session_set_receive_function(session_t *s, session_on_receive_handler function);
MLC_API int session_set_status_change_function(session_t *s, session_on_status_change_handler function);
MLC_API int session_getinfo(session_t *s, session_info_t *info);

MLC_API session_listener_t *session_listener_create(cycle_t *cycle, const mlc_addr_conf_t *addr);
MLC_API int session_listener_set_handler(session_listener_t *sl, void *data, session_on_connected_handler on_connected, session_on_receive_handler on_receive, session_on_status_change_handler on_status);

MLC_API int session_data_send(session_t *s, const char *data, uint32_t data_len);

MLC_API int session_recv(session_t *s, char *data, int data_len);
MLC_API void session_change_state(session_t *s,uint8_t state,int reason);
MLC_API int session_graceful_close(session_t *s,int reason);
int session_send_ctl(session_t *s, connection_t *c, uint8_t ctl_code);
session_t *session_get_by_si(cycle_t *cycle,sinfo_t *si);
void session_attach_connection(session_t *s,connection_t *c,uint8_t line);

#endif
