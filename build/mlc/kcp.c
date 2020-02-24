#include "kcp.h"
#include "core/encoding.h"
#include "core/chain.h"
#include "core/log.h"
#include "connection.h"
#include "session.h"
#include "fec.h"
#include "cycle.h"
#include "debugger.h"
#include "core/twnd.h"


#define TRACE_KCP(k,fmt,...) log_info(L_KCP(k->cycle),"[kcp]s=%d p=%p|"fmt,\
    ((session_t*)k->data)->index,k, ##__VA_ARGS__)

const char *KCP_CMD_STR[4] = {
    "1push",
    "2ack", 
    "3wask",
    "4wins",
};

const uint32_t KCP_RTO_NDL = 30;  // no delay min rto
const uint32_t KCP_RTO_MIN = 100; // normal min rto
const uint32_t KCP_RTO_DEF = 200;
const uint32_t KCP_RTO_MAX = 60000;
const uint32_t KCP_ASK_SEND = 1;  // need to send KCP_CMD_WASK
const uint32_t KCP_ASK_TELL = 2;  // need to send KCP_CMD_WINS
const uint32_t KCP_WND_SND = 1024;
const uint32_t KCP_WND_RCV = 1024; // must >= max fragment size
const uint32_t KCP_MTU_DEF = 1400;
const uint32_t KCP_ACK_FAST = 3;
const uint32_t KCP_INTERVAL = 100;
const uint32_t KCP_OVERHEAD = 18;
const uint32_t KCP_DEADLINK = 20;
const uint32_t KCP_THRESH_INIT = 2;
const uint32_t KCP_THRESH_MIN = 2;
const uint32_t KCP_PROBE_INIT = 7000;   // 7 secs to probe window size
const uint32_t KCP_PROBE_LIMIT = 120000; // up to 120 secs to probe window


const char * kcp_cmd_str(int cmd)
{
    if (cmd < 5 && cmd>=0) {
        return KCP_CMD_STR[cmd];
    }
    return "";
}

static inline uint32_t
_min_(uint32_t a, uint32_t b)
{
    return a <= b ? a : b;
}

static inline uint32_t _max_(uint32_t a, uint32_t b)
{
    return a >= b ? a : b;
}

static inline uint32_t _bound_(uint32_t lower, uint32_t middle, uint32_t upper)
{
    return _min_(_max_(lower, middle), upper);
}

static inline int32_t _diff_(uint32_t later, uint32_t earlier)
{
    return ((int32_t)(later - earlier));
}


static void kcp_update_rtt(kcp_pcb_t *kcp, int32_t rtt)
{
    int32_t rto = 0;
    if (kcp->rx_srtt == 0)
    {
        kcp->rx_srtt = rtt;
        kcp->rx_rttval = rtt / 2;
    }
    else
    {
        int32_t delta = rtt - kcp->rx_srtt;
        if (delta < 0)
        {
            delta = -delta;
        }

        kcp->rx_rttval = (3 * kcp->rx_rttval + delta) / 4;
        kcp->rx_srtt = (7 * kcp->rx_srtt + rtt) / 8;

        if (kcp->rx_srtt < 1)
        {
            kcp->rx_srtt = 1;
        }
    }
    rto = kcp->rx_srtt + _max_(kcp->interval, 4 * kcp->rx_rttval);
    int32_t last = kcp->rx_rto; 
    kcp->rx_rto = _bound_(kcp->rx_minrto, rto, KCP_RTO_MAX);
    if(abs(last - kcp->rx_rto) > 50){
        TRACE_KCP(kcp,"rto change kcp_srtt=%d kcp_rttval=%d min_rto=%d,rtt=%d rto=%d, %d=>%d",
            kcp->rx_srtt,kcp->rx_rttval, kcp->rx_minrto, rtt,rto,last,kcp->rx_rto);
    }

}


static void kcp_parse_ack(kcp_pcb_t *kcp, uint32_t sn,uint32_t ts)
{
    kcp_seg_data_t *seg = twnd_del(kcp->wnd_send,sn,1);
    if (seg) 
    {
        chain_release_one(seg->chain, mlc_chain_mask_kcp_write);
        seg->chain = NULL;
        TRACE_KCP(kcp,"sn=%d confirm send wnd=%d head=%u, dif=%d",sn, twnd_left_size(kcp->wnd_send), kcp->wnd_send->head ,kcp->current - ts);
    }
    else
    {
        TRACE_KCP(kcp,"sn=%d confirm too much una=%u", sn,kcp->wnd_send->head);
    }
}

static void kcp_parse_una(kcp_pcb_t *kcp, uint32_t una)
{
    twnd_t *wnd_send = kcp->wnd_send;
    uint32_t max_sn = mlc_min(una,wnd_send->tail);
    for(uint32_t sn = wnd_send->head; sn < max_sn; sn++){
        kcp_seg_data_t *seg = twnd_del(wnd_send,sn,1);
        if (seg) 
        {
            TRACE_KCP(kcp,"sn=%d confirm by una send wnd=%d head=%u, dif=%d",sn, twnd_left_size(wnd_send), wnd_send->head ,kcp->current - seg->ts);
            chain_release_one(seg->chain, mlc_chain_mask_kcp_write);
            seg->chain = NULL;
        }
    }    
}

static void kcp_parse_fastack(kcp_pcb_t *kcp, uint32_t sn_now)
{
    twnd_t *wnd_send = kcp->wnd_send;
    uint32_t max_sn = mlc_min(sn_now,wnd_send->tail);
    for (uint32_t sn = wnd_send->head; sn < max_sn; sn++)
    {
        kcp_seg_data_t *seg = twnd_get(wnd_send, sn);
        if (seg) {
            ++seg->fastack;
        }
    }
}

static chain_t *kcp_create_output_chain(kcp_pcb_t *kcp, uint8_t cmd, uint32_t sn, uint32_t ts, uint16_t datalen)
{
    chain_t *chain = chain_alloc(kcp->cycle->pool, chain_len_from_kcp(datalen), MLC_PKG_MAX_SIZE, mlc_chain_mask_kcp_write);
    if (chain == NULL)
    {
        TRACE_KCP(kcp,"chain full");
        return NULL;
    }

    mlc_pkg_data_t *data = chain_to_data(chain);
    data->pkg.type = MLC_PKG_TYPE_DATA;
    data->pkg.len = 0;

    mlc_pkg_kcp_t *kcpdata = &data->kcp;
    kcpdata->cmd = cmd;
    kcpdata->sn = sn;
    kcpdata->ts = ts;
    //kcpdata->len = chain_to_kcp_len(chain);
    kcpdata->una = kcp->wnd_recv->head;
    kcpdata->wnd = twnd_left_size(kcp->wnd_recv);
    return chain;
}


kcp_pcb_t *kcp_create(cycle_t *cycle,void *data)
{
    kcp_pcb_t *kcp = (kcp_pcb_t *)malloc(sizeof(kcp_pcb_t));
    assert(kcp);
    kcp->cycle = cycle;
    kcp->data = data;
    kcp->ts_recent = 0;
    kcp->ts_lastack = 0;
    kcp->ts_probe = 0;
    kcp->probe_wait = 0;
    kcp->snd_wnd = KCP_WND_SND;
    kcp->rcv_wnd = KCP_WND_RCV;
    kcp->rmt_wnd = KCP_WND_RCV;
    kcp->cwnd = 1;
    kcp->incr = 0;
    kcp->probe = 0;
    kcp->mtu = KCP_MTU_DEF;
    kcp->mss = kcp->mtu - KCP_OVERHEAD;
    kcp->stream = 0;

    kcp->nrcv_buf = 0;
    kcp->nsnd_buf = 0;
    kcp->nrcv_que = 0;
    kcp->nsnd_que = 0;
    kcp->state = 0;
    kcp->acklist = NULL;
    kcp->ackblock = 0;
    kcp->ackcount = 0;
    kcp->rx_srtt = 0;
    kcp->rx_rttval = 0;
    kcp->rx_rto = KCP_RTO_DEF;
    kcp->rx_minrto = KCP_RTO_MIN;
    kcp->current = 0;
    kcp->interval = KCP_INTERVAL;
    kcp->ts_flush = KCP_INTERVAL;
    kcp->nodelay = 0;
    kcp->updated = 0;
    kcp->ssthresh = KCP_THRESH_INIT;
    kcp->fastresend = 0;
    kcp->nocwnd = 0;
    kcp->xmit = 0;
    kcp->dead_link = KCP_DEADLINK;
    kcp->output = NULL;
    kcp->next_update = 0;
    kcp->ack_buf_n = 0;

    kcp->wnd_send = twnd_create(sizeof(kcp_seg_data_t), KCP_WND_SND, KCP_WND_SND);
    kcp->wnd_recv = twnd_create(sizeof(kcp_seg_data_t) ,KCP_WND_RCV, KCP_WND_RCV);

	kcp->chain_recv = NULL;
	kcp->chain_send = NULL;
    return kcp;
}

void kcp_release(kcp_pcb_t *kcp)
{
    if (kcp)
    {
        for (int i = kcp->wnd_send->head; i < kcp->wnd_send->tail; i++)
        {
            kcp_seg_data_t *seg = twnd_get(kcp->wnd_send, i);
            if (seg && seg->chain)
            {
                chain_release_one(seg->chain, mlc_chain_mask_kcp_write);
            }
        }
        twnd_destroy(kcp->wnd_send);
        kcp->wnd_send = NULL;

        for (int i = kcp->wnd_recv->head; i < kcp->wnd_recv->tail; i++)
        {
            kcp_seg_data_t *seg = twnd_get(kcp->wnd_recv, i);
            if (seg && seg->chain)
            {
                chain_release_one(seg->chain, mlc_chain_mask_kcp_read);
            }
        }
        twnd_destroy(kcp->wnd_recv);
        kcp->wnd_recv = NULL;
 
        kcp->nrcv_buf = 0;
        kcp->nsnd_buf = 0;
        kcp->nrcv_que = 0;
        kcp->nsnd_que = 0;
        kcp->ackcount = 0;

        kcp->acklist = NULL;
        free(kcp);
    }
}

int kcp_send_wnd_left(kcp_pcb_t *kcp)
{
    return twnd_left_size(kcp->wnd_send);
}

int kcp_send(kcp_pcb_t *kcp, chain_t *chain, uint8_t enable)
{
    mlc_pkg_kcp_t *kcp_pkg = chain_to_kcp(chain);
    if (enable == 0)
    {
        kcp_pkg->cmd = 0;
        kcp->output(chain, kcp->data);
        return 0;
    }

    if (twnd_left_size(kcp->wnd_send) <= 0)
    {
        TRACE_KCP(kcp,"kcp send failed, wnd too small rmt=%u,send=%u", kcp->rmt_wnd, twnd_left_size(kcp->wnd_send));
        return -1;
    }

    uint32_t cur_sn = kcp->wnd_send->tail;
    kcp_seg_data_t *data = twnd_add(kcp->wnd_send, cur_sn);
    if (data == NULL)
    {
        TRACE_KCP(kcp, "add to wnd error ");
        return -1;
    }

    kcp_pkg->cmd = KCP_CMD_PUSH;
    kcp_pkg->sn = cur_sn;
    kcp_pkg->ts = 0; //tobe set when flush
    chain_retain_one(chain, mlc_chain_mask_kcp_write);
    assert(chain->next == NULL);
    data->chain = chain;
    data->fastack = 0;
    data->t_resend = 0;
    TRACE_KCP(kcp, "kcp send sn=%u,wnd rmt=%u,send=%u", cur_sn, kcp->rmt_wnd, twnd_left_size(kcp->wnd_send));
    kcp->updated = 0;
    return 0;
}

void kcp_ack_push(kcp_pcb_t *kcp, uint32_t sn, uint32_t ts)
{
    if (kcp->ack_buf_n >= KCP_MAX_ACK_FRAME) {
        kcp_ack_flush(kcp);
    }
    
    kcp_ack_buf_t *ack_buf = &kcp->ack_buf[kcp->ack_buf_n++];
    ack_buf->sn = sn;
    ack_buf->ts = ts;
}


int kcp_input(kcp_pcb_t *kcp, chain_t *chain)
{
    mlc_pkg_kcp_t *p = chain_to_kcp(chain);
    uint8_t cmd = p->cmd;
    TRACE_KCP(kcp,"kcp recv cmd=%s,sn=%u,una=%u,wnd rmt=%u,send=%u",
          kcp_cmd_str(cmd), p->sn,p->una, kcp->rmt_wnd, twnd_left_size(kcp->wnd_send));
          
    if (cmd == 0)
    {
        kcp->input(chain, kcp->data);
        return 0;
    }

    if (cmd >= KCP_CMD_MAX)
    {
        TRACE_KCP(kcp,"unknow kcp cmd=%d",cmd);
        return 0;
    }
    kcp->rmt_wnd = p->wnd;
    kcp->updated = 0;//need sendback ack

    kcp_parse_una(kcp, p->una);

    if (cmd == KCP_CMD_ACK)
    {
        if (kcp->current >= p->ts)
        {
            kcp_update_rtt(kcp, kcp->current - p->ts);
        }

        kcp_parse_ack(kcp, p->sn,p->ts);
    }
    else if (cmd == KCP_CMD_PUSH)
    {
        //kcp_ack_push 必须在twnd_add 之前，否则flush会出事
        kcp_ack_push(kcp, p->sn, p->ts);
        kcp_seg_data_t *data = twnd_add(kcp->wnd_recv,p->sn);
        if(data == NULL)
        {
            TRACE_KCP(kcp,"recv wnd add faild sn=%d,",p->sn);
            return 0;
        }
        chain_retain_one(chain,mlc_chain_mask_kcp_read);
        assert(chain->next == NULL);
        data->chain = chain;
    }
    else if (cmd == KCP_CMD_WASK)
    {
        // ready to send back KCP_CMD_WINS in kcp_flush
        // tell remote my window size
        TRACE_KCP(kcp,"ask wnd");
        kcp->probe |= KCP_ASK_TELL;
    }
    else if (cmd == KCP_CMD_WINS)
    {
        TRACE_KCP(kcp,"remote tell me wnd=%d",p->wnd);
    }
    
    kcp_parse_fastack(kcp, mlc_max(p->sn,p->una));
    if (kcp->wnd_send->head > p->una)
    {
        uint32_t lastcwnd = kcp->cwnd;
        if (kcp->cwnd < kcp->rmt_wnd)
        {
            uint32_t mss = kcp->mss;
            if (kcp->cwnd < kcp->ssthresh)
            {
                kcp->cwnd++;
                kcp->incr += mss;
            }
            else
            {
                if (kcp->incr < mss)
                    kcp->incr = mss;
                kcp->incr += (mss * mss) / kcp->incr + (mss / 16);
                if ((kcp->cwnd + 1) * mss <= kcp->incr)
                {
                    kcp->cwnd++;
                }
            }
            if (kcp->cwnd > kcp->rmt_wnd)
            {
                kcp->cwnd = kcp->rmt_wnd;
                kcp->incr = kcp->rmt_wnd * mss;
            }
        }
        if(lastcwnd != kcp->cwnd){
            TRACE_KCP(kcp,"kcp cwnd change %u=>%u",lastcwnd,kcp->cwnd);
        }
    }

    twnd_t *wnd_recv = kcp->wnd_recv;
    for(uint32_t i = wnd_recv->head ; i < wnd_recv->tail; i++)
    {
        kcp_seg_data_t *seg = twnd_get(wnd_recv, i);
        if (seg && i == wnd_recv->head)
        {
            kcp->chain_recv = chain_append_one(kcp->chain_recv,seg->chain);
            twnd_del(wnd_recv, i, 0);
            seg->chain = NULL;
        }
        else
        {
            break;
        }
    }
    int cnt = 0;
    while(kcp->chain_recv)
    {
        chain_t *c = kcp->chain_recv;
        kcp->chain_recv = c->next;
        c->next = NULL;
        mlc_pkg_kcp_t *kcppkg = &chain_to_data(c)->kcp;
        int ret = kcp->input(c, kcp->data);
        if (ret < 0)
        {
            TRACE_KCP(kcp,"kcp oninput failed ret=%d", ret);
            c->next = kcp->chain_recv;
            kcp->chain_recv = c;
            break;
        }
        else
        {
            chain_release(c, mlc_chain_mask_kcp_read);
        }
        
        ++cnt;
    }
    return cnt;
}


uint32_t kcp_update(kcp_pcb_t *kcp, uint32_t current)
{
    int32_t slap;

    kcp->current = current;
    if (kcp->updated == 0)
    {
        kcp->updated = 1;
        kcp->ts_flush = kcp->current;
    }

    slap = _diff_(kcp->current, kcp->ts_flush);

    if (slap >= 10000 || slap < -10000)
    {
        kcp->ts_flush = kcp->current;
        slap = 0;
    }

    if (slap >= 0)
    {
        kcp->ts_flush += kcp->interval;
        if (_diff_(kcp->current, kcp->ts_flush) >= 0)
        {
            kcp->ts_flush = kcp->current + kcp->interval;
        }
        kcp_flush(kcp);
    }
    else
    {
        // TRACE_KCP("ignore");
    }
    
    return kcp->next_update;
}

void kcp_ack_flush(kcp_pcb_t *kcp)
{
    if (kcp->ack_buf_n > 0)
    {
        int sent_ack_n = 0;
        uint32_t una = kcp->wnd_recv->head;
        for(int i = 0; i < kcp->ack_buf_n; i++)
        {
            kcp_ack_buf_t *ack_buf = &kcp->ack_buf[i];
            //保证一定发送最后一个ack
            if (ack_buf->sn > una || ( sent_ack_n==0 && i == (kcp->ack_buf_n - 1) )) {
                ++sent_ack_n;
                chain_t *ack_chain = kcp_create_output_chain(kcp, KCP_CMD_ACK, ack_buf->sn, ack_buf->ts, 0);
                if (ack_chain)
                {
                    kcp->output(ack_chain, kcp->data);
                    chain_release_one(ack_chain, mlc_chain_mask_kcp_write);
                }
                else
                {
                    TRACE_KCP(kcp,"send ack failed");
                }
            }
        }
        TRACE_KCP(kcp,"ack_flush count=%u,sent=%d",kcp->ack_buf_n,sent_ack_n);
        kcp->ack_buf_n = 0;
    }
}


void kcp_output_tell(kcp_pcb_t *kcp)
{
    chain_t *chain = kcp_create_output_chain(kcp, KCP_CMD_WINS, 0, kcp->current,0);
    if (chain)
    {
        kcp->output(chain, kcp->data);
        chain_release_one(chain, mlc_chain_mask_kcp_write);
    }
}


void kcp_flush(kcp_pcb_t *kcp)
{
    uint32_t current = kcp->current;
    uint32_t size;
    uint32_t next_update = current + 100 * KCP_INTERVAL;
    if (kcp->next_update > current) {
        next_update = kcp->next_update;
    }

    cycle_t *cycle = kcp->cycle;

    // probe window size (if remote window size equals zero)
    if (kcp->rmt_wnd == 0)
    {
        if (kcp->probe_wait == 0)
        {
            kcp->probe_wait = KCP_PROBE_INIT;
            kcp->ts_probe = kcp->current + kcp->probe_wait;
        }
        else
        {
            if (_diff_(kcp->current, kcp->ts_probe) >= 0)
            {
                if (kcp->probe_wait < KCP_PROBE_INIT)
                    kcp->probe_wait = KCP_PROBE_INIT;
                kcp->probe_wait += kcp->probe_wait / 2;
                if (kcp->probe_wait > KCP_PROBE_LIMIT)
                    kcp->probe_wait = KCP_PROBE_LIMIT;
                kcp->ts_probe = kcp->current + kcp->probe_wait;
                kcp->probe |= KCP_ASK_SEND;
            }
        }
    }
    else
    {
        kcp->ts_probe = 0;
        kcp->probe_wait = 0;
    }

    kcp_ack_flush(kcp);

    // flush window probing commands
    if (kcp->probe & KCP_ASK_SEND)
    {
        chain_t *chain = kcp_create_output_chain(kcp,KCP_CMD_WASK,0,kcp->current,0);
        if (chain) 
        {
            kcp->output(chain, kcp->data);
            chain_release_one(chain,mlc_chain_mask_kcp_write);
        }
    }

    // flush window probing commands
    if (kcp->probe & KCP_ASK_TELL)
    {
        kcp_output_tell(kcp);
    }

    kcp->probe = 0;

    int fastack_n = 0;
    int resent_n = 0;

    // calculate window size
    // calculate resent
    uint32_t fast_ack_cnt = (kcp->fastresend > 0) ? (uint32_t)kcp->fastresend : 0xffffffff;
    uint32_t rtomin = (kcp->nodelay == 0) ? (kcp->rx_rto >> 3) : 0;

    uint32_t cwnd = (uint32_t)mlc_min(kcp->snd_wnd, (int)kcp->rmt_wnd);
    if (kcp->nocwnd == 0)
    {
        cwnd = mlc_min(kcp->cwnd, cwnd);
    }
    // cwnd = mlc_max(cwnd,1);
    twnd_t *wnd_send = kcp->wnd_send;
    int max_sn = mlc_min(wnd_send->tail, wnd_send->head + cwnd);
    uint32_t tcurrent = kcp->current;
    uint16_t rcv_wnd = twnd_left_size(kcp->wnd_recv);
    if(wnd_send->head<max_sn)
    {
        TRACE_KCP(kcp,"flush wnd cwnd=%u kcpcwnd=%u nocwnd=%u send [%u,%u),tail=%u", cwnd, kcp->cwnd,kcp->nocwnd, wnd_send->head, max_sn, wnd_send->tail);
    }
    for(uint32_t sn = wnd_send->head; sn < max_sn; sn++)
    {
        kcp_seg_data_t *seg = twnd_get(wnd_send, sn);
        if (seg) {
            char need_send = 0;
            char need_resend = 0;
            if (seg->t_resend == 0)
            {
                seg->xmit = 1;
                need_send = 1;
                seg->rto = kcp->rx_rto;
                seg->ts = current;
                seg->t_resend = current + kcp->rx_rto + rtomin;
                TRACE_KCP(kcp,"kcp first send sn=%d,rto=%u,kcp_rto=%u,resend=%u,rtomin=%u",
                    sn,seg->rto,kcp->rx_rto,seg->t_resend,rtomin);
            }
            else if (current >= seg->t_resend) {
                ++seg->xmit;
                need_resend = 1;
                seg->rto += kcp->rx_rto / 2;
                seg->t_resend = current + seg->rto;
                ++resent_n;
                session_t *s = kcp->data;
                TRACE_KCP(kcp,"s=%d,s_op=%d kcp resend, sn=%d rto=%u kcp_rto=%u resend=%u rtomin=%u delay=%u",
                    s->index,s->index_op, sn, seg->rto, kcp->rx_rto, seg->t_resend, rtomin, current - seg->ts);
            }
            else if (seg->fastack >= fast_ack_cnt)
            {
                ++seg->xmit;
                need_resend = 1;

                TRACE_KCP(kcp,"s=%d kcp resend by fastack, sn=%d rto=%u kcp_rto=%u resend=%u rtomin=%u, delay=%u fastack_n=%u kcp_fastresend=%u", ((session_t*)kcp->data)->index, sn, seg->rto, kcp->rx_rto, seg->t_resend, rtomin, current - seg->ts, seg->fastack, kcp->fastresend);
                
                seg->fastack = 0;
                seg->t_resend = current + seg->rto;
                ++fastack_n;
            }
            next_update = mlc_min(next_update,seg->t_resend);
            
            if (need_send || need_resend)
            {
                chain_t *c = seg->chain;
                if (need_resend) {
                    c = chain_clone_one(c,mlc_chain_mask_kcp_write);
                }
                mlc_pkg_kcp_t *kcpdata = &chain_to_data(c)->kcp;
                kcpdata->una = kcp->wnd_recv->head;
                kcpdata->wnd = rcv_wnd;
                kcpdata->ts = current;
                int ret = kcp->output(c, kcp->data);
                if (ret < 0)
                {
                    TRACE_KCP(kcp,"kcp output err ret=%d", ret);
                    break;
                }
                if(need_resend){
                    chain_release_one(c,mlc_chain_mask_kcp_write);
                }
                if (seg->xmit >= kcp->dead_link)
                {
                    TRACE_KCP(kcp,"kcp dead!!!!!!!");
                    if (kcp->dead) {
                        kcp->dead(kcp->data);
                        kcp->dead_link = 0;
                    }
                    // kcp->state = -1;
                }
            }
        }
    }

    // update ssthresh
    if (fastack_n)
    {
        uint32_t lastcwnd = kcp->cwnd;
        uint32_t inflight = (wnd_send->tail - wnd_send->head)/2;
        kcp->ssthresh = inflight < KCP_THRESH_MIN ? KCP_THRESH_MIN : inflight;
        kcp->cwnd = kcp->ssthresh + fast_ack_cnt;
        kcp->cwnd = mlc_max(kcp->cwnd, 1);
        kcp->incr = kcp->cwnd * kcp->mss;
        TRACE_KCP(kcp,"kcp cwnd change by fastack %u=>%u", lastcwnd, kcp->cwnd);
    }

    if (resent_n)
    {
        kcp->ssthresh = (cwnd / 2) < KCP_THRESH_MIN ? KCP_THRESH_MIN : (cwnd / 2);
        TRACE_KCP(kcp,"kcp cwnd change by resent %u=>%u", kcp->cwnd, 1);
        kcp->cwnd = 1;
        kcp->incr = kcp->mss;
    }
    kcp->next_update = next_update;
    // TRACE_KCP(kcp,"kcp next update %u=>%u",current,kcp->next_update);
}

int kcp_setmtu(kcp_pcb_t *kcp, uint32_t mtu)
{
    if (mtu < 50 || mtu < (int)KCP_OVERHEAD)
    {
        return -1;
    }

    kcp->mtu = mtu;
    kcp->mss = kcp->mtu - KCP_OVERHEAD;

    return 0;
}

int kcp_wndsize(kcp_pcb_t *kcp, uint32_t sndwnd, uint32_t rcvwnd)
{

    if (sndwnd > 0)
    {
        kcp->snd_wnd = sndwnd;
    }

    if (rcvwnd > 0)
    { 
        kcp->rcv_wnd = _max_(rcvwnd, KCP_WND_RCV);
    }

    return 0;
}

int kcp_waitsnd(kcp_pcb_t *kcp)
{
    return kcp->nsnd_buf + kcp->nsnd_que;
}

int kcp_interval(kcp_pcb_t *kcp, uint32_t interval)
{
    if (interval > 5000)
    {
        interval = 5000;
    }
    else if (interval < 10)
    {
        interval = 10;
    }
    kcp->interval = interval;
    return 0;
}

int kcp_nodelay(kcp_pcb_t *kcp, int32_t nodelay, uint32_t interval, int32_t resend, int32_t nc)
{
    if (nodelay >= 0)
    {
        kcp->nodelay = nodelay;
        if (nodelay)
        {
            kcp->rx_minrto = KCP_RTO_NDL;
        }
        else
        {
            kcp->rx_minrto = KCP_RTO_MIN;
        }
    }

    interval = mlc_min(interval,5000);
    interval = mlc_max(interval,10);
    kcp->interval = interval;

    if (resend >= 0)
    {
        kcp->fastresend = resend;
    }
    if (nc >= 0)
    {
        kcp->nocwnd = nc;
    }
    return 0;
}