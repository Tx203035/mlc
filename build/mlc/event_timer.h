#ifndef _MLC_EVENT_TIMER_H_
#define _MLC_EVENT_TIMER_H_


#include "mlc.h"


#define MLC_EVENT_TIMER_INTERVAL_MS 5
#define MLC_EVENT_TIMER_HUB_MS 15000
#define MLC_EVENT_TIMER_HUB_SLOT_N (MLC_EVENT_TIMER_HUB_MS / MLC_EVENT_TIMER_INTERVAL_MS)
// #define MLC_EVENT_TIMER_FRAME_OFFSET_MS 1

typedef int (*event_timer_handler_pt)(void *data);

struct event_timer_s
{
    cycle_t *cycle;
    event_timer_handler_pt handler;
    void *data;
    int32_t slot_index;
    uint64_t happen_ms;
    uint64_t period_ms;
    event_timer_t *next;
    event_timer_hub_t *hub;
    char repeated;
    char started;
};



struct event_timer_hub_s
{
    cycle_t *cycle;
    // uint64_t time_now_ms_offset; //带offset让同一帧产生的定时器有一定时移，避免过于集中的分布,每分配一次会增加，每帧重置
    uint64_t last_update_ms;
    int max_process_timer_n;
    event_timer_t *slot[MLC_EVENT_TIMER_HUB_SLOT_N];
};

MLC_API event_timer_hub_t *event_timer_hub_create(cycle_t *cycle);
MLC_API event_timer_t *event_timer_create(event_timer_hub_t *hub,void *data,event_timer_handler_pt handler);
MLC_API int event_timer_start(event_timer_t *timer, uint64_t delay, char repeated);
MLC_API int event_timer_stop(event_timer_t *timer);
MLC_API int event_timer_hub_process(event_timer_hub_t *hub);
MLC_API int event_timer_release(event_timer_t *timer);

#endif
