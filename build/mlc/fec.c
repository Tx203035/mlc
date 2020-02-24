#include "fec.h"
#include "core/encoding.h"
#include "core/util.h"
#include "core/chain.h"
#include "debugger.h"
#include "core/log.h"
#include "connection.h"
#include "core/twnd.h"
#include "core/crc.h"
#include "session.h"
#include "cycle.h"


#define TRACE_FEC(f,fmt,...) log_info(L_FEC(f->cycle),"[fec]s=%d p=%p|"fmt,\
    ((session_t*)f->data)->index, f, ##__VA_ARGS__)
#define TRACE_FEC_O(fmt,...) log_info(NULL,"[fec]|"fmt,##__VA_ARGS__)

typedef reed_solomon rs;
static rs *rs_table[FEC_DATA_SHARD_MAX][FEC_PARITY_SHARD_MAX];
static unsigned char **snd_shards;
static unsigned char **rcv_shards;
static unsigned char *rcv_marks;

#define FEC_RECONS_HEADER_SIZE (sizeof(mlc_pkg_fec_t) - mlc_offset_of(mlc_pkg_fec_t, block))


static void fec_packet_group_reset(fec_packet_group_t *pkt_group)
{
    TRACE_FEC_O("pkg_group %d %d", pkt_group->gn, pkt_group->sn);
    if (pkt_group)
    {
        for (int i = 0; i < FEC_SHARD_MAX; i++)
        {
            if (pkt_group->group[i])
            {
                TRACE_FEC_O("release group %d", i);
                chain_release_one(pkt_group->group[i], mlc_chain_mask_fec_group);
                pkt_group->group[i] = NULL;
            }
        }

        pkt_group->gn = 0;
        pkt_group->sn = 0;
        pkt_group->block_size = 0;
        pkt_group->data_cnt = 0;
        pkt_group->parity_cnt = 0;
        pkt_group->shard = 0;
    }
}

void fec_global_init()
{
    fec_init();

    for (int i = 0; i < FEC_DATA_SHARD_MAX; i++)
    {
        for (int j = 0; j <= i && j <= FEC_PARITY_SHARD_MAX; j++)
        {
            rs_table[i][j] = reed_solomon_new(i + 1, j + 1);
            assert(rs_table[i][j]);
        }
    }

    snd_shards = (unsigned char **)malloc(sizeof(unsigned char *) * FEC_SHARD_MAX);
    assert(snd_shards);
    rcv_shards = (unsigned char **)malloc(sizeof(unsigned char *) * FEC_SHARD_MAX);
    assert(rcv_shards);
    rcv_marks =  (unsigned char *)malloc(sizeof(unsigned char) * FEC_SHARD_MAX);
    assert(rcv_marks);
}

void fec_global_release()
{
    for (int i = 0; i < FEC_DATA_SHARD_MAX; i++)
    {
        for (int j = i; j <= i && j <= FEC_PARITY_SHARD_MAX; j++)
        {
            if (rs_table[i][j])
            {
                reed_solomon_release(rs_table[i][j]);
                rs_table[i][j] = NULL;
            }
        }
    }

    if (snd_shards)
    {
        free(snd_shards);
        snd_shards = NULL;
    }
    if (rcv_shards)
    {
        free(rcv_shards);
        rcv_shards = NULL;
    }
    if (rcv_marks)
    {
        free(rcv_marks);
        rcv_marks = NULL;
    }
}

fec_pcb_t *fec_create(cycle_t *cycle, void *data, uint8_t data_shards, uint8_t parity_shards)
{
    fec_pcb_t *fec = (fec_pcb_t *)malloc(sizeof(fec_pcb_t));
    fec->cycle = cycle;
    fec->data = data;
    fec_set_shard(fec, data_shards, parity_shards);
    fec->read_chain = NULL;
    fec->write_chain = NULL;
    fec->write_chain_n = 0;

    fec->snd_buf = (fec_packet_group_t *)malloc(sizeof(fec_packet_group_t));
    memset(fec->snd_buf, 0, sizeof(fec_packet_group_t));
    fec->snd_buf->shard = fec->shard;

    fec->rcv_buf = twnd_create(sizeof(fec_packet_group_t), FEC_RCV_LIMIT, FEC_RCV_LIMIT);
    return fec;
}

void fec_set_shard(fec_pcb_t *fec, uint8_t data_shards, uint8_t parity_shards)
{
    data_shards = data_shards > FEC_DATA_SHARD_MAX ? FEC_DATA_SHARD_MAX : data_shards;
    parity_shards = parity_shards > FEC_PARITY_SHARD_MAX ? FEC_PARITY_SHARD_MAX : parity_shards;
    parity_shards = parity_shards > data_shards ? data_shards : parity_shards;
    fec->shard = PACK_FEC_SHARD(data_shards, parity_shards);
}

void fec_release(fec_pcb_t *fec)
{
    if (fec)
    {
        if (fec->snd_buf)
        {
            fec_packet_group_reset(fec->snd_buf);
            free(fec->snd_buf);
            fec->snd_buf = NULL;
        }

        if (fec->rcv_buf)
        {
            for (int i = fec->rcv_buf->head; i < fec->rcv_buf->tail; i++)
            {
                fec_packet_group_t *grp = twnd_get(fec->rcv_buf, i);
                if (grp)
                {
                    fec_packet_group_reset(grp);
                }
            }
            twnd_destroy(fec->rcv_buf);
            fec->rcv_buf = NULL;
        }
        if (fec->write_chain) 
        {
            fec->write_chain = chain_release(fec->write_chain,mlc_chain_mask_fec_write);
        }
        fec->write_chain_n = 0;

        if(fec->read_chain)
        {
            fec->read_chain = chain_release(fec->read_chain,mlc_chain_mask_fec_read);
        }
        free(fec);
    }
}

int fec_input(fec_pcb_t *fec, chain_t *chain)
{
    mlc_pkg_fec_t *fec_pkg = chain_to_fec(chain);
    if (fec_pkg->shard == 0)
    {
        //直接发送模式，不经过FEC
        fec->input(chain, fec->data);
        return 0;
    }

    uint32_t gn = GET_FEC_GN(fec_pkg->gsn);
    uint8_t sn = GET_FEC_SN(fec_pkg->gsn);
    TRACE_FEC(fec, "fec_input shard=%x gn=%d sn=%d",fec_pkg->shard,gn,sn);
    if (gn < fec->rcv_buf->head)
    {
        TRACE_FEC(fec, "recv old pkg, gn:%d", gn);
        return 0;
    }

    fec_packet_group_t *pkt_group = twnd_get(fec->rcv_buf, gn);
    if (pkt_group == NULL)
    {
        pkt_group = twnd_add_force(fec->rcv_buf, gn, (twnd_destroy_func)fec_packet_group_reset);
        assert(pkt_group);
        fec_packet_group_reset(pkt_group);
        pkt_group->gn = gn;
        pkt_group->block_size = 0;
        pkt_group->data_cnt = 0;
        pkt_group->parity_cnt = 0;
        pkt_group->shard = fec_pkg->shard;
    }

    if ((pkt_group->data_cnt + pkt_group->parity_cnt) >= GET_FEC_DATA_SHARD(pkt_group->shard))
    {
        //这组包已经全部恢复出来了，后续的包不需要处理
        TRACE_FEC(fec,"data_cnt=%d part_cnt=%d", pkt_group->data_cnt,pkt_group->parity_cnt);
        return 0;
    }

    if (pkt_group->group[sn - 1] != NULL)
    {
        //重复包，不处理
        TRACE_FEC(fec,"repeated gn:%d, sn:%d", gn, sn);
        return 0;
    }

    chain_retain_one(chain, mlc_chain_mask_fec_group);
    pkt_group->group[sn - 1] = chain;

    uint8_t data_shard = GET_FEC_DATA_SHARD(pkt_group->shard);
    uint8_t parity_shard = GET_FEC_PARITY_SHARD(pkt_group->shard);
    uint8_t total_shard = data_shard + parity_shard;

    if (sn > data_shard)
    {
        pkt_group->parity_cnt++;
        pkt_group->block_size = mlc_max(pkt_group->block_size, chain_to_fec_len(chain));
    }
    else
    {
        // if (fec_pkg->crc != mlc_crc(fec_pkg->data, fec_pkg->len)) {
        //     TRACE_FEC(fec_pkg,"fec crc check failed indata=%u,calc=%u",fec_pkg->crc,mlc_crc(fec_pkg->data, fec_pkg->len));
        //     assert("fec crc check failed");
        // }
        chain_retain_one(chain, mlc_chain_mask_fec_read);
        fec->read_chain = chain_append_one(fec->read_chain, chain);
        pkt_group->data_cnt++;
    }

    if ((pkt_group->data_cnt + pkt_group->parity_cnt) >= data_shard && pkt_group->parity_cnt > 0)
    {
        rs *rs_pcb = rs_table[data_shard - 1][parity_shard - 1];
        memset(rcv_marks, 1, total_shard);

        chain_t *temp, *head = NULL;
        for (int i = 0; i < total_shard; i++)
        {
            chain_t *pkg_chain = pkt_group->group[i];
            mlc_pkg_fec_t *pkg_fec = chain_to_fec(pkg_chain);
            
            if (pkg_chain != NULL)
            {
                rcv_marks[i] = 0;
                rcv_shards[i] = (unsigned char *)(pkg_fec->block);
                // printf("\t[fec sn=%d]",i+1);
                // for (int n = 0; n < pkt_group->block_size + FEC_RECONS_HEADER_SIZE + 8; n++)
                // {
                //     printf("%02x", pkg_fec->block[n]);
                // }
                // printf("\n\tchain=%p,len=%d,%p\n", pkg_chain, pkt_group->block_size, pkg_fec->block);
            }
            else
            {
                if (i < data_shard)
                {
                    temp = chain_alloc(chain->pool, chain_len_from_fec(pkt_group->block_size), MLC_PKG_MAX_SIZE, mlc_chain_mask_fec_read);
                    head = chain_append_one(head, temp);
                    rcv_shards[i] = (unsigned char *)chain_to_fec(temp)->block;
                }
            }
        }
        reed_solomon_reconstruct(rs_pcb, rcv_shards, rcv_marks, total_shard, pkt_group->block_size + FEC_RECONS_HEADER_SIZE);

        temp = head;
        while (temp)
        {
            mlc_pkg_data_t *data = chain_to_data(temp);
            fec_pkg = &data->fec;
            data->pkg.type = MLC_PKG_TYPE_DATA;
            data->pkg.len = chain_to_connection_len(temp);
            pkg_debug(temp,"fec reconstruct pkg");
            // if (fec_pkg->crc != mlc_crc(fec_pkg->data, fec_pkg->len)) {
            //     TRACE_FEC(fec,"fec crc check failed indata=%u,calc=%u",fec_pkg->crc,mlc_crc(fec_pkg->data, fec_pkg->len));
            //     assert("fec crc check failed");
            // }            
            temp->len = chain_len_from_fec(fec_pkg->len);
            temp = temp->next;
        }

        fec->read_chain = chain_append(fec->read_chain, head);
    }

    int ret = -1;
    while (fec->read_chain)
    {
        chain = fec->read_chain;
        fec->read_chain = chain->next;
        chain->next = NULL;
        ret = fec->input(chain, fec->data);
        if (ret < 0)
        {
            TRACE_FEC(fec,"fec input ret < 0");
            chain->next = fec->read_chain;
            fec->read_chain = chain;
            break;
        }
        chain_release_one(chain, mlc_chain_mask_fec_read);
    }
    return 0;
}


static int fec_send_impl(fec_pcb_t *fec, chain_t *chain)
{
    chain_retain_one(chain, mlc_chain_mask_fec_group);
    fec_packet_group_t *snd_buf = fec->snd_buf;
    snd_buf->group[snd_buf->sn] = chain;
    mlc_pkg_fec_t *fec_pkg = chain_to_fec(chain);
    ++fec->snd_buf->sn;
    fec_pkg->gsn = PACK_FEC_GSN(fec->snd_buf->gn, fec->snd_buf->sn);
    fec_pkg->shard = snd_buf->shard;
    fec_pkg->len = chain_to_fec_len(chain);

    int ret = fec->output(chain, fec->data);
    if (ret < 0)
    {
        TRACE_FEC(fec,"fec output error ret=%d", ret);//fec的包是可以丢弃的，如果太忙就丢弃
    }

    //TRACE_FEC(fec, "fec send, gn=%d, gn=%d, sn=%d", fec->snd_buf->gn, GET_FEC_GN(fec_pkg->gsn), GET_FEC_SN(fec_pkg->gsn));

    snd_buf->data_cnt++;
    snd_buf->block_size = mlc_max(chain_to_fec_len(chain), snd_buf->block_size);
    uint8_t data_shard = GET_FEC_DATA_SHARD(snd_buf->shard);
    
    if (snd_buf->data_cnt >= data_shard)
    {
        uint8_t parity_shard = GET_FEC_PARITY_SHARD(fec->snd_buf->shard);
        chain_t *shard_chain[FEC_PARITY_SHARD_MAX];
        for (int i = 0; i < parity_shard; i++)
        {
            chain_t *temp = chain_alloc(chain->pool, chain_len_from_fec(snd_buf->block_size), MLC_PKG_MAX_SIZE, mlc_chain_mask_fec_write);
            //TRACE_FEC(fec,"fec shard blocksize=%u, chain=%p", snd_buf->block_size, temp);
            mlc_pkg_fec_t *p = chain_to_fec(temp);
            ++snd_buf->sn;
            p->gsn = PACK_FEC_GSN(snd_buf->gn, snd_buf->sn);
            p->shard = snd_buf->shard;
            //p->crc = 0;
            snd_shards[i + data_shard] = (unsigned char *)p->block;
            shard_chain[i] = temp;
        }

        for (int i = 0; i < data_shard; i++)
        {
            fec_pkg = chain_to_fec(snd_buf->group[i]);
            snd_shards[i] = (unsigned char *)(fec_pkg->block);
        }

        rs *rs_pcb = rs_table[data_shard - 1][parity_shard - 1];
        reed_solomon_encode2(rs_pcb, snd_shards, data_shard + parity_shard, snd_buf->block_size + FEC_RECONS_HEADER_SIZE);

        for (int i = 0; i < parity_shard; i++)
        {
            chain_t *c = shard_chain[i];
            int ret = fec->output(c, fec->data);
            if (ret < 0)
            {
                TRACE_FEC(fec,"fec output error ret=%d", ret); //fec的包是可以丢弃的，如果太忙就丢弃
            }
            chain_release_one(c,mlc_chain_mask_fec_write);
        }

        uint16_t next_gn = ++snd_buf->gn;
        fec_packet_group_reset(snd_buf);
        snd_buf->gn = next_gn;
        snd_buf->shard = fec->shard;
    }
    return 0;
}

int fec_send(fec_pcb_t *fec, chain_t *chain, uint8_t enable)
{
    if (enable == 0)
    {
        //直接发送模式，没有经过FEC
        mlc_pkg_fec_t *fec_pkg = chain_to_fec(chain);
        fec_pkg->shard = 0;
        fec->output(chain, fec->data);
    }
    else
    {
        chain_retain_one(chain, mlc_chain_mask_fec_write);
        fec->write_chain = chain_append_one(fec->write_chain, chain);
        ++fec->write_chain_n;
    }
    return 0;
}

int fec_flush(fec_pcb_t *fec) 
{
    int ret = fec->write_chain_n;
    if (fec->write_chain) 
    {
        fec_packet_group_t *snd_buf = fec->snd_buf;
        uint8_t data_shard = GET_FEC_DATA_SHARD(snd_buf->shard); 
        uint8_t parity_shard = GET_FEC_PARITY_SHARD(snd_buf->shard);
        uint8_t end_data_shard = (fec->write_chain_n + snd_buf->sn ) % data_shard;
        uint8_t end_parity_shard = mlc_min(end_data_shard,parity_shard);
        uint8_t end_flag = PACK_FEC_SHARD(end_data_shard, end_parity_shard);

        while(fec->write_chain)
        {
            chain_t *chain = fec->write_chain;
            fec->write_chain = chain->next;
            chain->next = NULL;
            if (fec->write_chain_n <= end_data_shard)
            {
                snd_buf->shard = end_flag;
            }
            int ret = fec_send_impl(fec, chain);
            if (ret < 0) 
            {
                TRACE_FEC(fec,"fec send fail ret=%d",ret);
            }
            chain_release_one(chain,mlc_chain_mask_fec_write);
            --fec->write_chain_n;
        }
        assert(snd_buf->sn == 0);
        assert(snd_buf->shard == fec->shard);
        assert(fec->write_chain_n == 0);
        // TRACE_FEC(fec,"fec_flush sent=%d",ret);
    }
    return ret;
}
