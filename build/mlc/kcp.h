#ifndef __KCP_H__
#define __KCP_H__

#include "mlc.h"

#define KCP_MAX_ACK_FRAME 32

typedef int (*kcp_inout_handler)(chain_t *chain, void *data);
typedef int (*kcp_dead_handler)(void *data);
enum
{
    KCP_CMD_PUSH = 1,
    KCP_CMD_ACK = 2,
    KCP_CMD_WASK = 3,
    KCP_CMD_WINS = 4,
    KCP_CMD_MAX = 5,
};


typedef struct kcp_seg_data_s
{
    chain_t *chain;
    uint32_t fastack;
    uint32_t t_resend;
    uint32_t rto;
    uint32_t ts;
    uint8_t xmit;
}kcp_seg_data_t;


typedef struct kcp_ack_buf_s
{
    uint32_t sn;
    uint32_t ts;
}kcp_ack_buf_t;


struct kcp_pcb_s
{
    void *data;
    cycle_t *cycle;
    uint32_t mtu, mss, state;
    uint32_t snd_nxt, rcv_nxt;
    uint32_t ts_recent, ts_lastack, ssthresh;
    int32_t rx_rttval, rx_srtt, rx_rto, rx_minrto;
    uint32_t snd_wnd, rcv_wnd, rmt_wnd, cwnd, probe;
    uint32_t current, interval, ts_flush, xmit;
    uint32_t nrcv_buf, nsnd_buf;
    uint32_t nrcv_que, nsnd_que;
    uint32_t nodelay, updated;
    uint32_t ts_probe, probe_wait;
    uint32_t dead_link, incr;
    uint32_t *acklist;
    uint32_t ackcount;
    uint32_t ackblock;
    int32_t fastresend;
    int32_t nocwnd, stream;
    kcp_inout_handler output;
    kcp_inout_handler input;
    kcp_dead_handler dead;
    twnd_t *wnd_send;
    twnd_t *wnd_recv;
    chain_t *chain_recv;
    chain_t *chain_send;
    uint32_t next_update;
    kcp_ack_buf_t ack_buf[KCP_MAX_ACK_FRAME];
    uint32_t ack_buf_n;
};

MLC_API kcp_pcb_t *kcp_create(cycle_t *cycle,void *data);

MLC_API void kcp_release(kcp_pcb_t *kcp);


MLC_API int kcp_send(kcp_pcb_t *kcp, chain_t *chain, uint8_t enable);

MLC_API int kcp_input(kcp_pcb_t *kcp, chain_t *chain);

MLC_API chain_t *kcp_recv(kcp_pcb_t *kcp);

MLC_API uint32_t kcp_update(kcp_pcb_t *kcp, uint32_t current);

MLC_API void kcp_flush(kcp_pcb_t *kcp);
MLC_API void kcp_output_tell(kcp_pcb_t *kcp);

MLC_API int kcp_peeksize(kcp_pcb_t *kcp);

MLC_API int kcp_setmtu(kcp_pcb_t *kcp, uint32_t mtu);

MLC_API int kcp_wndsize(kcp_pcb_t *kcp, uint32_t sndwnd, uint32_t rcvwnd);

MLC_API int kcp_waitsnd(kcp_pcb_t *kcp);

MLC_API int kcp_nodelay(kcp_pcb_t *kcp, int32_t nodelay, uint32_t interval, int32_t resend, int32_t nc);

MLC_API const char * kcp_cmd_str(int cmd);

void kcp_ack_push(kcp_pcb_t *kcp, uint32_t sn, uint32_t ts);
void kcp_ack_flush(kcp_pcb_t *kcp);

int kcp_send_wnd_left(kcp_pcb_t *kcp);

#endif