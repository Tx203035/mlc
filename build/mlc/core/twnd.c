#include "twnd.h"
#include "util.h"

twnd_t *twnd_create(size_t data_len, int wnd_total, int wnd_init)
{
    twnd_t *wnd = malloc(sizeof(twnd_t));
    if (wnd == NULL) 
    {
        return NULL;
    }
    wnd->wnd_total = wnd_total; 
    twnd_set_max(wnd, wnd_init);
    wnd->head = 0;
    wnd->tail = 0;
    wnd->data_len = mlc_align(data_len + sizeof(twnd_slot_t));
    size_t totallen = wnd_total * wnd->data_len;
    wnd->pool = malloc(totallen);
    memset(wnd->pool, 0, totallen);
    wnd->wnd_max = wnd_init;
    return wnd;
}

int twnd_destroy(twnd_t *wnd)
{
    free(wnd);
    return 0;
}

static inline twnd_slot_t *twnd_get_slot(twnd_t *wnd, uint32_t sn)
{
    twnd_slot_t *p = (twnd_slot_t *)((char *)wnd->pool + wnd->data_len * (sn % wnd->wnd_total));
    return p;
}

void *twnd_get(twnd_t *wnd, uint32_t sn)
{
    if (sn < wnd->head || sn >= wnd->tail)
    {
        return NULL;
    }

    twnd_slot_t *slot = twnd_get_slot(wnd, sn);
    if (slot->sn != sn || slot->inuse==0) 
    {
        return NULL;
    }
    else
    {
        return (void *)slot->data;
    }
}

void *twnd_add_force(twnd_t *wnd, uint32_t sn, twnd_destroy_func fun_del)
{
    if (sn < wnd->head)
    {
        return NULL;
    }

    wnd->tail = mlc_max(sn + 1, wnd->tail);
    uint32_t new_head = (wnd->tail - wnd->head) > wnd->wnd_max ? wnd->tail - wnd->wnd_max : wnd->head;

    for (uint32_t i = wnd->head; i < new_head; i++)
    {
        twnd_slot_t *slot = twnd_get_slot(wnd, i);
        if (slot->sn != 0 && fun_del != NULL)
        {
            fun_del(slot->data);
        }
        slot->sn = 0;
        slot->inuse = 0;
    }
    wnd->head = new_head;

    twnd_slot_t *slot = twnd_get_slot(wnd, sn);
    if (slot->sn != 0 && fun_del != NULL)
    {
        fun_del(slot->data);
    }
    slot->sn = sn;
    slot->inuse = 1;
    return (void *)slot->data;
}

void *twnd_add(twnd_t *wnd, uint32_t sn)
{
    if (sn < wnd->head || sn - wnd->head + 1 > wnd->wnd_max)
    {
        return NULL;
    }

    twnd_slot_t *slot = twnd_get_slot(wnd, sn);
    if (slot->sn != 0 || slot->inuse)
    {
        return NULL;
    }

    slot->sn = sn;
    slot->inuse = 1;
    wnd->tail = mlc_max(sn + 1, wnd->tail);
    return (void *)slot->data;
}

void *twnd_del(twnd_t *wnd, uint32_t sn, int auto_del)
{
    if (sn < wnd->head || sn >= wnd->tail) 
    {
        return NULL;
    }

    twnd_slot_t *slot = twnd_get_slot(wnd, sn);
    if (slot->sn != sn || slot->inuse==0) 
    {
        return NULL;
    }

    slot->sn = 0;
    slot->inuse = 0;
    if (sn == wnd->head)
    {
        wnd->head += 1;
    }

    if (auto_del)
    {
        for (uint32_t i = wnd->head; i < wnd->tail; i++)
        {
            twnd_slot_t *s = twnd_get_slot(wnd, i);
            if (s->inuse == 0)
            {
                wnd->head += 1;
            }
            else
            {
                break;
            }
        }
    }

    return slot->data;
}


