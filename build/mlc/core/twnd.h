#ifndef __LIBMLC_H__
#define __LIBMLC_H__
#include "core.h"

typedef int (*twnd_destroy_func)(void *data);

struct twnd_slot_s
{
    uint32_t sn;
    int32_t inuse;
    char data[0];
};

struct twnd_s
{
    uint32_t head;
    uint32_t tail;
    int wnd_max;
    int data_len;
    int wnd_total;
    twnd_slot_t *pool;
    twnd_destroy_func func_destroy;
};

twnd_t *twnd_create(size_t data_len, int wnd_total, int wnd_init);
int twnd_destroy(twnd_t *wnd);

void *twnd_add_force(twnd_t *wnd, uint32_t sn, twnd_destroy_func fun_del);
void *twnd_add(twnd_t *wnd, uint32_t sn);
void *twnd_del(twnd_t *wnd, uint32_t sn, int auto_del);
void *twnd_get(twnd_t *wnd, uint32_t sn);

static inline int twnd_left_size(twnd_t *wnd)
{
    int size = (int)wnd->tail - (int)wnd->head;
    return wnd->wnd_max - (int)(size >= 0 ? size : ( wnd->wnd_total + size) );
}

static inline int twnd_set_max(twnd_t *wnd, int wnd_max)
{
    if (wnd_max > 0 && wnd_max < wnd->wnd_total)
    {
        wnd->wnd_max = wnd_max;
    }
    else
    {
        wnd->wnd_max = wnd->wnd_total;
    }
    
    return wnd->wnd_max;
}

#endif