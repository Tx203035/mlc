#include "sinfo.h"
#include "cycle.h"
#include "core/pool.h"

static inline uint32_t sinfo_gen_token(sinfo_collect_t *sc)
{
    return ++sc->match_counter;
}


sinfo_collect_t *sinfo_collect_create(cycle_t *cycle)
{
    sinfo_collect_t *sc = malloc(sizeof(sinfo_collect_t)); 
    assert(sc);
    if(!sc)
    {
        return NULL;
    }

    const cycle_conf_t *conf = &cycle->conf;
    sc->cycle = cycle;
    sc->pool = cycle->pool_small;
    sc->match_counter = 0;
    sc->map_gc_walk_index = 0;
    sc->auto_expire = 10000;
    imap_init(&sc->map,sc->pool);
    int ret = imap_set_size(&sc->map,2 * conf->connection_n);
    assert(ret >= 0);

    if(ret < 0)
    {
        free(sc);
        return NULL;
    }
    return sc;
}

int sinfo_collect_destroy(sinfo_collect_t *sc)
{
    imap_iter_t iter = imap_iter(&sc->map);
    sinfo_collect_t *si = imap_next(&sc->map,&iter);
    for(;si;si = imap_next(&sc->map,&iter) )
    {
        mlc_pfree(si->pool,si);
    }

    imap_deinit(&sc->map);
    free(sc);
    return 0;
}

int sinfo_collect_update(sinfo_collect_t *sc)
{
    imap_base_t *mb = &sc->map.base;
    imap_node_t *next, *node;
    uint64_t now_ms = sc->cycle->time_now_ms;
    uint64_t expire = sc->auto_expire;
    if(sc->map_gc_walk_index >= mb->nbuckets)
    {
        sc->map_gc_walk_index = 0;
    }
    int cnt = 0;
    while(sc->map_gc_walk_index < mb->nbuckets && cnt<10)
    {
        node = mb->buckets[sc->map_gc_walk_index++];
        while (node) {
            next = node->next;
            sinfo_t *si = node->value;
            assert(si);
            if(si && si->s==NULL && (now_ms - si->atime) >= expire )
            {
                uint32_t match = si->match;
                imap_remove(&sc->map,match);
                log_info(NULL,"match=%x expired left=%d",match,mb->nnodes);
                mlc_pfree(sc->pool,si);
                break;// the node and next be break,cannot go on
            }
            node = next;
        }
        ++cnt;
    }
    
    return cnt;
}


void sinfo_collect_trace(logger_t *l,sinfo_collect_t *sc)
{
    log_trace(l,"sinfo collect map_node=%d",sc->map.base.nnodes);
}

sinfo_t *sinfo_find(sinfo_collect_t *sc,uint32_t match)
{
    sinfo_t *si = imap_get(&sc->map,match);
    if(si)
    {
        si->atime = sc->cycle->time_now_ms;
    }
    return si;
}

void sinfo_delete(sinfo_collect_t *sc, uint32_t match)
{
    sinfo_t *si = imap_get(&sc->map,match);
    if(si)
    {
        imap_remove(&sc->map,match);
        mlc_pfree(sc->pool,si);
    }
}

sinfo_t *sinfo_create(sinfo_collect_t *sc)
{
    imap_sinfo_t *map = &sc->map;
    sinfo_t *si = mlc_palloc(sc->pool,sizeof(sinfo_t));
    if(si)
    {
        uint32_t token = sinfo_gen_token(sc);
        si->match = token;
        si->atime = sc->cycle->time_now_ms;
        si->s = NULL;
        int ret = imap_set(map,token,si);
        assert(ret==0);//ret==1 is found in map
        if(ret != 0)
        {
            mlc_pfree(sc->pool,si);
            return NULL;
        }
    }
    return si;
}