#include "event_timer.h"
#include <stdlib.h>
#include "cycle.h"
#include "event_timer.h"
#include "core/log.h"
#include "core/util.h"

static inline int find_slot(uint64_t t){
    return (int)((t / MLC_EVENT_TIMER_INTERVAL_MS) % MLC_EVENT_TIMER_HUB_SLOT_N);
}

event_timer_hub_t *event_timer_hub_create(cycle_t *cycle)
{
    event_timer_hub_t *hub = malloc(sizeof(event_timer_hub_t));
    assert(hub);
    memset(hub,0,sizeof(*hub));
    hub->cycle = cycle;
    // hub->time_now_ms_offset = cycle->time_now_ms;
    hub->last_update_ms = cycle->time_now_ms - MLC_EVENT_TIMER_INTERVAL_MS;
    return hub;
}


event_timer_t *event_timer_create(event_timer_hub_t *hub,void *data,event_timer_handler_pt handler)
{
    event_timer_t *timer = malloc(sizeof(event_timer_t));
    assert(timer);
    memset(timer,0,sizeof(event_timer_t));
    timer->cycle = hub->cycle;
    timer->hub = hub;
    timer->data = data;
    timer->handler = handler;
    timer->started = 0;
    timer->slot_index = -1;
    return timer;
}


int event_timer_start(event_timer_t *timer, uint64_t delay, char repeated)
{
    assert(timer->started == 0 && timer->slot_index==-1);
    event_timer_hub_t *hub = timer->hub;
    assert(timer->started == 0);
    assert(timer->next == NULL);
    delay = mlc_min(delay, MLC_EVENT_TIMER_HUB_MS - MLC_EVENT_TIMER_INTERVAL_MS);
    timer->repeated = repeated;
    timer->period_ms = delay;
    timer->happen_ms = hub->last_update_ms + delay;
    // hub->time_now_ms_offset += MLC_EVENT_TIMER_FRAME_OFFSET_MS;
    // assert(timer->happen_ms - hub->time_now_ms_offset < MLC_EVENT_TIMER_HUB_MS);
    int i = find_slot(timer->happen_ms);
    // log_info(NULL,"slot=%d",i);
    timer->slot_index = i;
    timer->next = hub->slot[i];
    hub->slot[i] = timer;
    timer->started = 1;
    return 0;
}

int event_timer_stop(event_timer_t *timer)
{
    assert(timer->started && timer->slot_index>=0);
    event_timer_hub_t *hub = timer->hub;
    int i = timer->slot_index;
    event_timer_t *t = hub->slot[timer->slot_index];
    event_timer_t *last = NULL;
    int found = 0;
    while(t)
    {
        if (t == timer) {
            found = 1;
            if (last == NULL)
            {
                hub->slot[i] = t->next;
            }
            else
            {
                last->next = t->next;
            }
            break;
        }
        last = t;
        t = t->next;
    }
    assert(found);
    timer->started = 0;
    timer->next = NULL;
    timer->slot_index = -1;
    return 0;
}


int event_timer_hub_process(event_timer_hub_t *hub)
{
    int need_process_per_frame = 1024;
    event_timer_t *need_process[1024];
    cycle_t *cycle = hub->cycle;
    uint64_t now_ms = cycle->time_now_ms;
    // hub->time_now_ms_offset = now_ms;
    int last_index = find_slot(hub->last_update_ms);
    int now_index = find_slot(now_ms);
    if (last_index == now_index) {
        return 0;
    }
    if (now_index < last_index)
    {
        now_index += MLC_EVENT_TIMER_HUB_SLOT_N;
    }
    // log_info(NULL,"%lu %d %lu %d", hub->last_update_ms, last_index, now_ms, now_index);
    int np_n = 0;
    for(int cur_index = last_index; cur_index < now_index ;cur_index++)
    {
        int i = cur_index % MLC_EVENT_TIMER_HUB_SLOT_N;
        event_timer_t *t = hub->slot[i];
        event_timer_t *last = NULL;
        while(t)
        {
            if (np_n < need_process_per_frame)
            {
                last = t;
                t = t->next;
                last->next = NULL;
                assert(last->slot_index == i);
                last->slot_index = -1;
                assert(last->started);
                // for(int x=0;x<np_n;x++)
                // {
                //     assert(last!=need_process[x]);
                // }
                need_process[np_n++] = last;
            }
            else
            {
                assert(0);
                log_info(NULL,"need more timer buffer now=%d",np_n);
                break;
            }
        }
        hub->slot[i] = t;
    }

    hub->last_update_ms = now_ms;
    if (np_n > hub->max_process_timer_n)
    {
        hub->max_process_timer_n = np_n;
    }
    
    for(int i = 0; i< np_n ; i++)
    {
        event_timer_t *timer = need_process[i];
        assert(timer->started);
        timer->started = 0;
        timer->happen_ms = 0;
        if(timer->repeated)
        {
            event_timer_start(timer, timer->period_ms, 1);
        }      
    }

    for(int i = 0; i< np_n ; i++)
    {
        event_timer_t *timer = need_process[i];
        int ret = timer->handler(timer->data);
        if (ret < 0) 
        { 
            log_info(NULL,"timer func ret=%d",ret);
        }
    }

    return np_n;
}

int event_timer_release(event_timer_t *timer)
{
    free(timer);
    return 0;
}


