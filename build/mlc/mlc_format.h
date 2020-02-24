#ifndef _MLC_FORMAT_H_
#define _MLC_FORMAT_H_

#include <stdint.h>
#include "core/chain.h"
#include "core/util.h"
#include "core/export.h"

#pragma pack(push, 1)

enum
{
    MLC_PKG_TYPE_CTL = 1,
    MLC_PKG_TYPE_DATA = 2,
};

enum mlc_control_e
{
    mlc_control_connect_req = 1,
    mlc_control_connect_rsp = 2,
    mlc_control_attach_req = 3,
    mlc_control_attach_rsp = 4,
    mlc_control_close_req = 5,
    mlc_control_close_rsp = 6,
    mlc_control_heartbeat = 7,
};

typedef struct mlc_pkg_session_s
{
    uint8_t health;
    uint8_t frg;
    uint32_t sn;
    char data[0];
} mlc_pkg_session_t;

typedef struct mlc_pkg_s
{
    uint16_t len;
    uint32_t match;
    uint16_t index;
    //TODO:下面三个可以合为一个
    uint8_t line;
    uint8_t health;
    uint8_t type;
    char data[0];
} mlc_pkg_t;

typedef struct mlc_pkg_body_ctl_s
{
    uint8_t control;
    uint8_t line;
    uint16_t index;
    uint16_t index_op;
    uint64_t ping_us;
    uint64_t ping_start_us;
} mlc_pkg_body_ctl_t;

typedef struct mlc_pkg_kcp_s
{
    char start[0];
    uint8_t cmd;
    uint16_t wnd;
    uint32_t ts;
    uint32_t sn;
    uint32_t una;
    //uint16_t len;
    char data[0];
} mlc_pkg_kcp_t;

typedef struct mlc_pkg_fec_s
{
    char start[0];
    uint32_t gsn;
    //uint8_t sn;
    uint8_t shard;
    char block[0];
    uint16_t len;
    //uint32_t crc;
    char data[0];
} mlc_pkg_fec_t;

typedef struct mlc_pkg_data_s
{
    struct mlc_pkg_s pkg;
    struct mlc_pkg_fec_s fec;
    struct mlc_pkg_kcp_s kcp;
    struct mlc_pkg_session_s session;
    char data[0];
} mlc_pkg_data_t;

typedef struct mlc_pkg_ctl_s
{
    struct mlc_pkg_s pkg;
    struct mlc_pkg_session_s session;
    struct mlc_pkg_body_ctl_s body;
} mlc_pkg_ctl_t;

#pragma pack(pop)

MLC_API void pkg_debug(const chain_t *chain,const char *prefix);

static inline mlc_pkg_t *chain_to_pkg(chain_t *chain)
{
    return (mlc_pkg_t *)(chain->data_start);
}

static inline mlc_pkg_ctl_t *chain_to_ctl(chain_t *chain)
{
    return (mlc_pkg_ctl_t *)(chain->data_start);
}

static inline mlc_pkg_body_ctl_t *chain_to_ctl_body(chain_t *chain)
{
    return &chain_to_ctl(chain)->body;
}

static inline mlc_pkg_data_t *chain_to_data(chain_t *chain)
{
    return (mlc_pkg_data_t *)(chain->data_start);
}

static inline mlc_pkg_session_t *chain_to_session(chain_t *chain)
{
    return &chain_to_data(chain)->session;
}

static inline mlc_pkg_fec_t *chain_to_fec(chain_t *chain)
{
    return &chain_to_data(chain)->fec;
}

static inline mlc_pkg_kcp_t *chain_to_kcp(chain_t *chain)
{
    return &chain_to_data(chain)->kcp;
}

static inline uint16_t chain_to_fec_len(chain_t *chain)
{
    return chain->len - mlc_offset_of(mlc_pkg_data_t, fec.data);
}

static inline uint16_t chain_to_connection_len(chain_t *chain)
{
    return chain->len - mlc_offset_of(mlc_pkg_data_t, pkg.data);
}

static inline uint16_t chain_to_kcp_len(chain_t *chain)
{
    return chain->len - mlc_offset_of(mlc_pkg_data_t, kcp.data);
}

static inline uint16_t chain_to_session_len(chain_t *chain)
{
    return chain->len - mlc_offset_of(mlc_pkg_data_t, session.data);
}

static inline uint16_t chain_len_from_kcp(uint16_t len)
{
    return len + mlc_offset_of(mlc_pkg_data_t, kcp.data);
}

static inline uint16_t chain_len_from_session(uint16_t len)
{
    return len + mlc_offset_of(mlc_pkg_data_t, session.data);
}

static inline uint16_t chain_len_from_fec(uint16_t len)
{
    return len + mlc_offset_of(mlc_pkg_data_t, fec.data);
}

#endif