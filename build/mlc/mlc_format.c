#include "mlc_format.h"
#include "core/log.h"
#include "fec.h"
#include "kcp.h"



void pkg_debug(const chain_t *c, const char *prefix)
{
    return;
    chain_t *chain = (chain_t *)c;
    const mlc_pkg_t *pkg = chain_to_pkg(chain);
    log_info(NULL,"[%s] chain=%p, chain_total_len=%d\n", prefix, chain, chain->len);

    printf("\t[pkg(%zu)]type=%d, len=%d index=%d health=%d\n",
           sizeof(*pkg),pkg->type, pkg->len, pkg->index, pkg->health);

    if (pkg->type != MLC_PKG_TYPE_CTL)
    {
        const mlc_pkg_data_t *d = chain_to_data(chain);
        const mlc_pkg_kcp_t *kcp = &d->kcp;
        const mlc_pkg_session_t *session = &d->session;
        const struct mlc_pkg_fec_s *fec = &d->fec;
        int is_shard = GET_FEC_SN(fec->gsn) > GET_FEC_DATA_SHARD(fec->shard);

        if (chain->len >= mlc_offset_of(mlc_pkg_data_t, fec.data))
        {
            printf("\t[fec(%zu)]gn=%d,sn=%d,shard=0x%x,len=%d,shard=%d\n",
                   sizeof(*fec), GET_FEC_GN(fec->gsn), GET_FEC_SN(fec->gsn), fec->shard, fec->len, is_shard);
       }
       if (0)
       {
              printf("\t[fec block %p]",fec->block);
              int fecdata_len = chain->len - mlc_offset_of(mlc_pkg_data_t, fec.block);
              for(int i = 0;i<fecdata_len+8;i++)
              {
                     printf("%02x", fec->block[i]);
              }
              printf("\n");
        }

        if (chain->len >= mlc_offset_of(mlc_pkg_data_t, kcp.data) && is_shard == 0)
        {
            printf("\t[kcp(%zu)]cmd=%s,wnd=%d,ts=%u,sn=%d,una=%d\n",
                   sizeof(*kcp),kcp_cmd_str(kcp->cmd), kcp->wnd, kcp->ts, kcp->sn, kcp->una);
        }
        if (chain->len >= mlc_offset_of(mlc_pkg_data_t, session.data) && is_shard == 0)
        {
            printf("\t[session(%zu)] frg=%d, health=%d\n",
                   sizeof(*session), session->frg, session->health);
       //      printf("\t[data]%s\n", session->data);
        }
    }
    else
    {
        const mlc_pkg_ctl_t *ctl = chain_to_ctl(chain);
        const mlc_pkg_session_t *session = &ctl->session;        
        const mlc_pkg_body_ctl_t *body = &ctl->body;

        printf("\t[session(%zu)]health=%d\n",
               sizeof(mlc_pkg_session_t), session->health);

        printf("\t[ctlbody(%zu)]contrl=%d, ping_us=%" PRIu64 ",ping_start_us=%" PRIu64 ",index=%d,index_op=%d\n",
               sizeof(mlc_pkg_body_ctl_t),body->control, body->ping_us, body->ping_start_us, body->index, body->index_op);
    }
}