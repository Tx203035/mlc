#ifndef _LIB_MLC_H_
#define _LIB_MLC_H_

#include "mlc_format.h"
#include "mlc_conf.h"
#include "core/core.h"

#define MLC_DATA_ALIGNMENT 8
#define MLC_DEFAULT_MTU  1460
#define MLC_PKG_MAX_SIZE  MLC_DEFAULT_MTU
#define MLC_DATA_MAX_SIZE 0xFFFF
#define MLC_MAX_FRAGMENT_CNT (MLC_DATA_MAX_SIZE / MLC_DEFAULT_MTU + 1) 

enum mlc_state_e
{
    mlc_state_init = 0,
    mlc_state_connecting = 1,
    mlc_state_connected = 2,
    mlc_state_disconnect = 3,
    mlc_state_lost = 4,
    mlc_state_close_wait = 5,
    mlc_state_closed = 6,
};


MLC_FORWARD_DECLARATION_STRUCT(connection)
MLC_FORWARD_DECLARATION_STRUCT(event)
MLC_FORWARD_DECLARATION_STRUCT(cycle)
MLC_FORWARD_DECLARATION_STRUCT(cycle_conf)
MLC_FORWARD_DECLARATION_STRUCT(connection_listener)
MLC_FORWARD_DECLARATION_STRUCT(session)
MLC_FORWARD_DECLARATION_STRUCT(session_info)
MLC_FORWARD_DECLARATION_STRUCT(session_listener)
MLC_FORWARD_DECLARATION_STRUCT(session_handler)
MLC_FORWARD_DECLARATION_STRUCT(rs_conf)
MLC_FORWARD_DECLARATION_STRUCT(mlc_addr_conf)
MLC_FORWARD_DECLARATION_STRUCT(debugger_cmd)
MLC_FORWARD_DECLARATION_STRUCT(session_data_body_ctl)
MLC_FORWARD_DECLARATION_STRUCT(event_timer)
MLC_FORWARD_DECLARATION_STRUCT(event_timer_hub)
MLC_FORWARD_DECLARATION_STRUCT(kcp_header)
MLC_FORWARD_DECLARATION_STRUCT(fec_header)

MLC_FORWARD_DECLARATION_STRUCT(mlc_tunnel_conf)
MLC_FORWARD_DECLARATION_STRUCT(mlc_tunnel)
MLC_FORWARD_DECLARATION_STRUCT(mlc_tunnel_ins)
MLC_FORWARD_DECLARATION_STRUCT(sinfo_collect)
MLC_FORWARD_DECLARATION_STRUCT(sinfo)


typedef int (*connection_on_accept_handler)(connection_t *c, void *data);

#endif