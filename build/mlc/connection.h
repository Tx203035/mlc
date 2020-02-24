#ifndef _CONNECTION_H_
#define _CONNECTION_H_

#include "mlc.h"

#define MLC_CONECTION_CHECK_LOST_MS 5000

enum
{
    MLC_CONNECT_MODE_TCP = 1,
    MLC_CONNECT_MODE_UDP = 2,
};

#pragma pack(1)
struct connection_header_s
{
    uint16_t len;
    uint8_t flag;
    uint32_t index;
    uint16_t match;
};
#pragma pack()

typedef int (*connection_on_read_handler)(connection_t *conn, void *data, chain_t *chain);
typedef int (*connection_on_status_handler)(connection_t *conn, uint8_t state);

struct connection_listener_s
{
    cycle_t *cycle;
    int fd;
    uint8_t mode;
    void *data;
    sockaddr_t socket_listen;
    event_t *event;
    connection_on_accept_handler on_accept;
    connection_on_read_handler on_read;
    connection_on_status_handler on_status;
    chain_t *temp_chain;
};

struct connection_s
{
    cycle_t *cycle;
    uint32_t index;
    uint32_t index_op;
    uint32_t match;
    uint8_t state;
    uint8_t mode;
    int fd;
    uint64_t last_active_ms;
    uint64_t start_time_ms;
    void *userdata;
    session_t *session;
    event_t *event;

    chain_t *write_chain;
    chain_t *read_chain;
    chain_t *temp_chain;
    uint16_t write_len;

    uint8_t active;
    connection_on_read_handler on_read;
    connection_on_status_handler on_status;
    sockaddr_t peer_addr;
    uint64_t ping_us;
    struct sockaddr *sockaddr;
    int socklen;

    uint8_t raw;
    uint8_t attached;
    uint8_t reconnect_n;
    

    //status
    uint32_t sent_n;
    uint32_t recv_n;
};

int connection_init(connection_t *conn, void *init_conf, int index);
int connection_reset(connection_t *conn);
int connection_release(connection_t *conn);
int connection_close(connection_t *conn, int reason);
void connection_set_raw(connection_t *conn);

connection_t *connection_create(cycle_t *cycle, void *data, const char *ip_addr, unsigned short port, int mode);
int connection_set_read_function(connection_t *conn, connection_on_read_handler on_read);
int connection_set_status_function(connection_t *conn, connection_on_status_handler on_status);
int connection_reconnect(connection_t *conn);
int connection_data_send(connection_t *conn, chain_t *chain, uint8_t type);

connection_listener_t *create_udp_listener(cycle_t *cycle, const mlc_addr_conf_t *addr, void *data);
connection_listener_t *create_tcp_listener(cycle_t *cycle, const mlc_addr_conf_t *addr, void *data);
int connection_listener_set_handler(connection_listener_t *cl, connection_on_accept_handler on_accept);

#endif