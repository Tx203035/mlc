#ifndef _MLC_CORE_H_
#define _MLC_CORE_H_

#include "export.h"

#include <inttypes.h>

#if defined __APPLE__
    #include "osx_inc.h"
#elif defined _MSC_VER
    #include "windows_inc.h"
#else
    #include "linux_inc.h"
#endif

#include "error_code.h"

typedef union {
    struct sockaddr sockaddr;
    struct sockaddr_in sockaddr_in;
    struct sockaddr_in6 sockaddr_in6;
} sockaddr_t;

#define MLC_STATIC_ASSERT(x, msg) _Static_assert(x, msg)

#define MLC_FORWARD_DECLARATION_STRUCT(x) \
    struct x##_s;                         \
    typedef struct x##_s x##_t;

MLC_FORWARD_DECLARATION_STRUCT(logger)
MLC_FORWARD_DECLARATION_STRUCT(mlc_chain_pool)
MLC_FORWARD_DECLARATION_STRUCT(mlc_pool)
MLC_FORWARD_DECLARATION_STRUCT(mlc_pool_data_node)
MLC_FORWARD_DECLARATION_STRUCT(pool)
MLC_FORWARD_DECLARATION_STRUCT(chain)
MLC_FORWARD_DECLARATION_STRUCT(fec_pcb)
MLC_FORWARD_DECLARATION_STRUCT(fec_packet)
MLC_FORWARD_DECLARATION_STRUCT(fec_packet_group)
MLC_FORWARD_DECLARATION_STRUCT(kcp_pcb)
MLC_FORWARD_DECLARATION_STRUCT(kcp_packet)
MLC_FORWARD_DECLARATION_STRUCT(tpool)
MLC_FORWARD_DECLARATION_STRUCT(tpool_obj)
MLC_FORWARD_DECLARATION_STRUCT(twnd)
MLC_FORWARD_DECLARATION_STRUCT(twnd_slot)
MLC_FORWARD_DECLARATION_STRUCT(data_buffer)
MLC_FORWARD_DECLARATION_STRUCT(dqueue_ptr)

#ifdef WINDOWS

#define MLC_EAGAIN WSAEWOULDBLOCK
#define MLC_EWOULDBLOCK WSAEWOULDBLOCK
#define MLC_EINPROGRESS EINPROGRESS
#define MLC_EINTR EINTR

#else

#define MLC_EAGAIN EAGAIN
#define MLC_EWOULDBLOCK EWOULDBLOCK
#define MLC_EINPROGRESS EINPROGRESS
#define MLC_EINTR EINTR

#endif

#endif