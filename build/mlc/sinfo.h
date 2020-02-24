#ifndef _MLC_SINFO_H_
#define _MLC_SINFO_H_

#include "mlc.h"
#include "core/imap.h"

struct sinfo_s
{
    uint32_t match;
    session_t *s;
    uint64_t atime;
};


typedef imap_t(struct sinfo_s *) imap_sinfo_t;

struct sinfo_collect_s
{
    cycle_t *cycle; 
    mlc_pool_t *pool;
    uint32_t match_counter;
    uint64_t auto_expire;
    unsigned map_gc_walk_index;
    imap_sinfo_t map;
};

sinfo_collect_t *sinfo_collect_create(cycle_t *cycle);
int sinfo_collect_destroy(sinfo_collect_t *sc);
void sinfo_collect_trace(logger_t *l,sinfo_collect_t *sc);

sinfo_t *sinfo_find(sinfo_collect_t *sc,uint32_t match);
sinfo_t *sinfo_create(sinfo_collect_t *sc);
void sinfo_delete(sinfo_collect_t *sc, uint32_t match);

int sinfo_collect_destroy(sinfo_collect_t *sc);
int sinfo_collect_update(sinfo_collect_t *sc);
#endif