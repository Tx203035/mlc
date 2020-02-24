#ifndef __FEC_H_
#define __FEC_H_

#include "mlc.h"
#include "core/rs.h"
#include "core/queue.h"
#include "core/export.h"

#define GET_FEC_DATA_SHARD(shard)   (shard >> 4)
#define GET_FEC_PARITY_SHARD(shard)   (shard & 0x0f)
#define PACK_FEC_SHARD(data_shard, parity_shard) ((parity_shard & 0xf) + (data_shard << 4))

#define GET_FEC_GN(gsn)  (gsn >> 5)
#define GET_FEC_SN(gsn)  (gsn & 0x1f)
#define PACK_FEC_GSN(gn, sn) ((gn << 5) + (sn & 0x1f))

#define FEC_DATA_SHARD_MAX   15
#define FEC_PARITY_SHARD_MAX  8
#define FEC_SHARD_MAX (FEC_DATA_SHARD_MAX + FEC_PARITY_SHARD_MAX)
#define FEC_RCV_LIMIT  5

typedef int (*fec_inout_handler)(chain_t *chain, void *data);

struct fec_packet_group_s
{
    chain_t *group[FEC_SHARD_MAX];
    uint16_t gn;
    uint8_t sn;
    uint8_t shard;
    uint16_t block_size;
    uint8_t data_cnt;
    uint8_t parity_cnt;
};

struct fec_pcb_s
{
    cycle_t *cycle;
    void *data;
    fec_inout_handler output;
    fec_inout_handler input;
    uint8_t shard;
    fec_packet_group_t *snd_buf;
    twnd_t *rcv_buf;
    chain_t *write_chain;
    chain_t *read_chain;
    uint32_t write_chain_n;
};

//只能在最开始调用一次
MLC_API void fec_global_init();
MLC_API void fec_global_release();

MLC_API fec_pcb_t *fec_create(cycle_t *cycle, void *data, uint8_t data_shards, uint8_t parity_shards);

MLC_API void fec_set_shard(fec_pcb_t *fec, uint8_t data_shards, uint8_t parity_shards);

MLC_API void fec_release(fec_pcb_t* fec);

MLC_API int fec_input(fec_pcb_t *fec, chain_t *chain);

MLC_API int fec_send(fec_pcb_t *fec, chain_t *chain, uint8_t enable);

MLC_API int fec_flush(fec_pcb_t *fec);

#endif


